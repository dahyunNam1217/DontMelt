# 엔진 기본 PTT 처리가 게임의 음성 송신 상태를 덮어쓰는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 음성 감지 모드 원격 클라이언트가 맵에 들어온 직후, 말해도 전송되지 않을 수 있었다 |
| 원인 | 엔진이 접속 초기화 때 `ClientEnableNetworkVoice(false)`로 송신을 끄는데, 게임 서브시스템은 "송신 중"이라는 캐시를 그대로 들고 있었다 |
| 해결 | 엔진의 기본 RPC 구현을 오버라이드해, 송신 판단 주체를 `UDMVoiceChatSubsystem` 하나로 모았다 |
| 관련 코드 | `ADMLobbyPlayerController`, `ADMMatchPlayerController`의 `ClientEnableNetworkVoice_Implementation` |

## 현상

RPC 도착 순서에 따라 생기는 문제라 항상 재현되지는 않는다. 호스트는 RPC가 로컬에서 즉시 실행돼 순서가 달라지므로 재현되지 않고, 원격 클라이언트에서만 날 수 있다. 엔진 동작을 추적하다가 발견했다.

## 원인 추적

게임은 송신 여부를 캐시해 실제로 바뀔 때만 엔진 API를 호출한다.

```cpp
// UDMVoiceChatSubsystem::UpdateNetworkVoiceTransmission()
const bool bShouldTransmit = IsBackendInitialized() && bPolicyAllowsTransmission && bInputAllowsTransmission;

// 실제 상태가 바뀔 때만 엔진 API를 호출한다.
if (bNetworkVoiceTransmitting == bShouldTransmit) { return; }

bShouldTransmit ? OnlineVoice->StartNetworkedVoice(LocalUserNum)
                : OnlineVoice->StopNetworkedVoice(LocalUserNum);
bNetworkVoiceTransmitting = bShouldTransmit;
```

그런데 게임 코드가 직접 부르지 않는 엔진 경로가 하나 더 있었다.

```text
DefaultGame.ini: [/Script/Engine.GameSession] bRequiresPushToTalk=true

AGameModeBase::GenericPlayerInitialization()
  → PC->ClientEnableNetworkVoice(!RequiresPushToTalk())   // = false
     → APlayerController::ClientEnableNetworkVoice_Implementation → ToggleSpeaking(false)
        → StopNetworkedVoice()
```

| 순서 | 사건 | 실제 송신 | 서브시스템 캐시 |
|---|---|---|---|
| 1 | PC BeginPlay → 음성 감지 모드라 Start | ON | true |
| 2 | 서버의 `ClientEnableNetworkVoice(false)` 도착 → Stop | **OFF** | **true** |
| 3 | 이후 Update 호출 | 캐시가 true라 "변화 없음"으로 판단 | true |

중복 API 호출을 줄이려던 캐시가, **외부에서 실제 상태를 바꾸는 경로**를 만나 불일치 상태로 굳는 구조였다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. `bRequiresPushToTalk=false` | 엔진이 접속 시 송신을 켜게 함 | PTT 사용자도 접속 즉시 송신이 켜져 PTT가 무력화된다 |
| B. 캐시 제거 | 매번 Start/Stop 호출 | 두 주체가 같은 상태를 제어하는 구조는 그대로라, 어느 값이 이길지가 여전히 순서에 달린다 |
| C. 엔진 호출 뒤 재동기화 | Super 호출 후 캐시를 리셋하고 Update | 가능하지만 잠시라도 엔진 값이 적용된다 |
| **D. 엔진 기본 동작 차단** | `_Implementation` 오버라이드, Super 미호출 | **채택.** 송신 제어 주체를 하나로 만든다. 이 프로젝트는 엔진의 `StartTalking`/`StopTalking` 경로를 쓰지 않아 잃는 기능이 없다 |

```cpp
// DMMatchPlayerController.h (Lobby도 동일)
/**
 * 엔진 기본 구현(ToggleSpeaking)은 GameSession::bRequiresPushToTalk 값으로
 * 네트워크 음성 전송을 강제로 켜고 끄므로 UDMVoiceChatSubsystem의 전송 상태와 어긋난다.
 * 음성 전송 판단은 VoiceChatSubsystem에만 맡긴다.
 */
virtual void ClientEnableNetworkVoice_Implementation(bool bEnable) override;
```

```cpp
void ADMMatchPlayerController::ClientEnableNetworkVoice_Implementation(const bool bEnable)
{
    // 엔진이 PostLogin 시 보내는 ClientEnableNetworkVoice(!bRequiresPushToTalk)가
    // 음성 감지 모드의 전송을 꺼 버리지 않도록 Super(ToggleSpeaking)를 호출하지 않는다.
    // 실제 전송 여부는 UDMVoiceChatSubsystem::UpdateNetworkVoiceTransmission()이 결정한다.
}
```

로비와 경기 맵의 PlayerController가 다르므로 두 클래스 모두에 적용했다. `Client, Reliable` RPC의 `_Implementation`은 UHT가 virtual로 생성하므로 서브클래스에서 오버라이드할 수 있다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 음성 감지 모드 클라이언트가 로비 입장 직후 키 입력 없이 말하기 | 다른 계정에서 들림 |
| 경기 맵 진입 직후 | 같음 |
| PTT 모드에서 키를 누르지 않음 | 송신되지 않음 |

## 정리

- 상태 캐시는 **그 상태를 바꾸는 주체가 하나일 때만** 안전하다. 엔진 기본 동작처럼 보이지 않는 주체가 있는지 확인해야 한다.
- 엔진 프레임워크를 쓸 때는 게임 코드가 부르지 않아도 **엔진이 대신 부르는 경로**(접속 초기화, 기본 RPC)까지 추적한다.

---

[Debugging 목차](../README.md)
