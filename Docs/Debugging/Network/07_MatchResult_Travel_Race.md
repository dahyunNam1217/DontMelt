# 경기 결과를 받기 전에 로비로 이동하던 문제와 ACK 설계

| 항목 | 내용 |
|---|---|
| 문제 | 결과 확정 직후 로비로 이동하면 일부 원격 클라이언트가 지난 경기 결과를 보관하지 못했다 |
| 원인 | 결과 복제(GameState)와 이동 명령(PlayerController)은 서로 다른 Actor 경로라 도착 순서가 보장되지 않는다 |
| 해결 | 결과마다 `ResultId`를 발급하고, 클라이언트가 **캐시를 마친 뒤** ACK를 보내면 서버가 전원 확인 후 이동하도록 했다. 이후 ACK 호출부가 빠져 있던 결함도 찾아 연결했다 |
| 관련 코드 | `ADMMatchGameState::HandleMatchResultChanged`, `ADMMatchPlayerController::NotifyMatchResultCached`, `ADMMatchGameMode::CommitServerTravel` |

## 현상

Listen Server에서 결과가 확정된 직후 호스트가 로비 복귀를 누르면, 느린 클라이언트는 로비에서 "지난 경기 결과"가 비어 보였다. 맵이 바뀌면 경기 GameState는 사라지므로, 결과는 이동 전에 GameInstance에 옮겨 두어야 한다.

```text
서버: 결과 확정
  ├─ GameState.MatchResult 복제       (Actor A의 프로퍼티)
  └─ PlayerController를 통한 맵 이동   (Actor B의 RPC)
원격 Client: 이동 명령을 먼저 처리 → GameInstance 결과 캐시 누락
```

서버가 순서대로 호출했다고 클라이언트가 같은 순서로 받는다고 볼 수 없다. 서로 다른 Actor의 복제와 RPC 사이에는 순서 보장이 없다.

## 원인 추적과 설계

기존 코드는 결과 확정 뒤 `ForceNetUpdate()`를 호출했지만, 이것은 "빨리 보내 달라"는 요청이지 "받았다"는 확인이 아니다. 고정 대기 시간을 넣어도 지연이 그보다 길면 다시 실패한다.

확인 시점도 따져 봤다. 클라이언트가 **결과를 받은 시점**이 아니라 **GameInstance에 캐시를 마친 시점**이어야 맵 이동 뒤에도 결과가 남는다.

```text
서버 SetMatchResult(ResultId)
  → Client OnRep_MatchResult
  → GameInstance.CacheFinalMatchResult()           ← 보관 성공
  → ServerAcknowledgeMatchResult(ResultId)         (Reliable)
  → 서버: 참가한 원격 클라이언트 전원의 ACK 확인
  → ExecuteServerTravel()
```

| 설계 요소 | 역할 |
|---|---|
| `FGuid ResultId` | 이전 경기의 늦은 ACK가 현재 경기 이동에 섞이지 않게 결과를 구분한다 |
| `CommitServerTravel()` | 전원 확인 시 이동하고, 미확인자가 있으면 URL을 보관한 채 대기한다 |
| `bResultTravelPending` | 대기 중 로비 복귀·재경기 요청이 중복으로 들어오는 것을 막는다 |
| `ResultAckTimeoutSeconds = 5` | 무한 대기를 막는 상한. 결과 전달을 보장하는 값이 아니라 응답 없는 클라이언트를 위한 탈출구다 |

대기 중 참가자가 퇴장하거나 서버가 복구 모드로 바뀌면 대기 상태와 타이머도 함께 정리한다.

## 후속 결함: 폴백에 가려진 ACK 호출 누락

이후 전체 소스를 검수하면서 **호출 0건 검색**으로 결함을 하나 더 찾았다.

```text
NotifyMatchResultCached        → 선언 1건, 정의 1건, 호출 0건
ServerAcknowledgeMatchResult   → NotifyMatchResultCached 내부에서만 호출
```

ACK를 보내는 함수는 있었지만 아무도 부르지 않았다. 원격 클라이언트가 한 명이라도 있으면 전원 확인 조건이 항상 false였고, 모든 결과 이동이 5초 타임아웃 경로로만 진행됐다. 게임은 결국 넘어가니 **겉으로는 정상처럼 보였다.** 타임아웃 폴백이 결함을 가리고 있던 셈이다.

| 안 | 내용 | 판단 |
|---|---|---|
| A. 결과 위젯에서 전송 | 결과 화면 갱신 마지막에 ACK 호출 | 기각. WBP 바인딩과 표시 조건에 따라 ACK가 빠질 수 있고, ACK의 의미가 "보관 완료"에서 "화면 표시"로 바뀐다 |
| **B. 결과 캐시 직후 전송** | `HandleMatchResultChanged()`에서 캐시 성공 시 전송 | **채택.** "보관 완료"라는 사실이 확정되는 데이터 계층에 직접 연결된다 |

```cpp
// DMMatchGameState.cpp - HandleMatchResultChanged()
bool bResultCached = false;

if (IsValid(GameInstance))
{
    // 보관 성공 여부를 결과 ACK 전송 조건으로 사용한다.
    bResultCached = GameInstance->CacheFinalMatchResult(this);
}

// 원격 클라이언트는 결과를 로컬에 보관한 뒤에만 서버에 ACK를 보낸다.
// Authority(Listen 호스트)는 ACK 대상이 아니며,
// 같은 ResultId 중복 전송은 NotifyMatchResultCached() 내부에서 걸러진다.
if (bResultCached && !HasAuthority() && MatchResult.ResultId.IsValid() && IsValid(World))
{
    if (ADMMatchPlayerController* LocalController =
            Cast<ADMMatchPlayerController>(World->GetFirstPlayerController()))
    {
        LocalController->NotifyMatchResultCached(MatchResult.ResultId);
    }
}
```

`CacheFinalMatchResult()`는 이미 성공 여부를 반환하고, `NotifyMatchResultCached()` 안에는 로컬 여부·Authority·ResultId 중복 검사가 있었다. 새 상태 없이 호출 한 곳만 추가하면 됐다.

`ResultId`를 검사하는 과정에서 정상 종료 경로가 ID를 발급하지 않던 회귀도 찾았다. 이 내용은 [ResultId 검사 회귀](08_MatchResult_ResultId_Regression.md)에 따로 정리했다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| `NetEmulation.PktLag 500`, 2~4인 경기 종료 직후 로비 이동 | 각 클라이언트의 캐시 로그가 Travel보다 먼저 찍힘 |
| 같은 조건의 서버 로그 | 원격 클라이언트 수만큼 ACK 수신, 타임아웃 경고 없음 |
| ACK 대기 중 클라이언트 퇴장 | 대기 해제 후 이동, 타이머 정리 |
| 로비 진입 후 | 모든 클라이언트에서 지난 경기 결과 표시 |

## 정리

- 서로 다른 Actor의 복제와 RPC는 **도착 순서를 가정하지 않는다.** 순서가 필요하면 확인 응답으로 만든다.
- 폴백이 있는 시스템은 폴백만으로 돌아가도 정상처럼 보인다. "기능이 되는가"가 아니라 **"설계한 정상 경로를 타는가"** 를 확인해야 한다.
- RPC는 정의만 보지 말고 **호출 지점이 실제로 있는지**까지 추적한다.

---

[Debugging 목차](../README.md)
