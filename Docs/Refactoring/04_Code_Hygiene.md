# 코드 위생과 배포 전 정리

기능 결함은 아니지만, 다음 사람이 코드를 고치거나 로그로 문제를 추적할 때 **판단을 틀리게 만드는 요소**와, 패키징·Shipping 환경에서만 드러나는 요소를 모아 정리했다. 항목마다 "왜 이 값인가"와 "바뀌면 안 되는 것이 바뀌지 않았는가"를 확인했다.

| 항목 | 분류 | 조치 |
|---|---|---|
| 유물 상한 `5` | 매직 넘버 | `DMMatch::MaxRelicCount`로 의미 부여 |
| ACK 대기 분기의 "Traveling" 로그 | 잘못된 로그 | 실제 동작대로 수정 |
| LF 파일 2개 | 줄바꿈 혼재 | CRLF 통일, 내용 불변 확인 |
| CP949 파일 2개 | 인코딩 혼재 | UTF-8 BOM 통일, 내용 불변 확인 |
| 헤더 줄 끝 `\` | 전처리 함정 | 제거 |
| 테스트 Exec 명령 | Shipping 포함 | 본문 `#if !UE_BUILD_SHIPPING` |
| 재경기 맵 경로 | PIE 접두사 | `UWorld::RemovePIEPrefix` |
| `ShutdownQuestSystem()` 중복 호출 | 중복 실행 | 한 번만 호출 |
| 로그 Wrapper 도입 | 설계 검토 | 도입하지 않음 |

## 유물 상한 `5` — 결함 의심이 오탐이었던 경우

`ADMMatchGameState`의 유물 공개 상태와 연장전 소개 상태 검증에 `5`가 그대로 들어 있었다.

```cpp
|| NewState.Entries.Num() > 5
|| Entry.RegisteredRelicCount > 5
```

처음에는 "데이터 에셋에서 유물을 늘리면 연출이 조용히 사라지는 결함"으로 의심했다. 확인해 보니 유물 생성 코드가 **진영마다 1개 + 공용 1개**로 고정돼 있어 최대값이 정확히 `4 + 1 = 5`였다. 값은 맞았고, **그 의미가 코드에 드러나지 않는 것**이 문제였다. 의심만으로 동작을 바꾸지 않고, 의미만 상수로 드러냈다.

```cpp
// DMMatchConstants.h
/** 한 경기에 존재할 수 있는 최대 유물 수. 진영마다 1개 + 공용 1개다. */
inline constexpr int32 MaxRelicCount = MaxPlayers + 1;
```

최대 인원이 바뀌면 상한도 함께 바뀐다.

## 반대 의미의 로그

결과 ACK를 **기다리는** 분기에서 이동을 **시작한다**는 로그를 찍고 있었다.

```cpp
// 결과를 아직 받지 못한 참가자가 있으면 이동을 보류한다.
bResultTravelPending = true;
// ...
UE_LOG(LogDMMatchGameMode, Log, TEXT("All results acknowledged. Traveling. URL=%s"), *TravelURL);
```

이동 지연을 로그로 추적하면 "모두 확인됐는데 왜 안 넘어가지?"라는 잘못된 방향으로 조사하게 된다. 로그는 사후 디버깅의 첫 번째 증거라, **틀린 로그는 없는 로그보다 해롭다.**

```cpp
UE_LOG(LogDMMatchGameMode, Log, TEXT("Waiting for result acknowledgements before travel. URL=%s"), *TravelURL);
```

## 줄바꿈과 인코딩 통일

프로젝트 소스는 UTF-8 BOM + CRLF가 기준이다. 바이트 단위로 전체를 검사해 예외 파일을 찾았다.

| 파일 | 문제 | 조치 |
|---|---|---|
| `DMRelicRevealHUDWidget.h/.cpp` | 파일 전체가 LF | CRLF 변환 |
| `DMBaseZone.cpp`, `DMRelic.cpp` | CP949 | UTF-8 BOM + CRLF 변환 |

처음 검사에서는 LF 파일을 "혼재"로 봤지만, 다시 확인하니 두 파일 전체가 LF였다. 변환은 다음 순서로 검증했다.

1. 모든 파일을 UTF-8로 디코딩해 보고, 실패한 파일만 CP949로 다시 읽었다.
2. 변환 후 BOM과 줄바꿈을 제거하고 원본과 비교해 **내용이 100% 같음**을 확인했다(논리 변경 0줄).
3. 변환 뒤 프로젝트 전체 `.h/.cpp/.cs`를 다시 검사해 CRLF가 아닌 파일이 0개임을 확인했다.

한글은 주석에만 있어 빌드는 되지만, 인코딩이 섞이면 다른 코드 페이지 환경에서 C4819 경고가 나거나 에디터 툴팁의 한글이 `����`처럼 깨질 수 있다. 줄바꿈 변환은 Git에서 전체 변경처럼 보이므로 `git diff --ignore-cr-at-eol`로 확인하고, **기능 수정과 다른 커밋**으로 분리했다.

## 헤더의 줄 연속 문자

```cpp
TWeakObjectPtr<UDMJellyPhysicsComponent>
    LocalObservedKillerPhysicsComponent;\     // ← 줄 끝 '\'
```

다음 줄이 빈 줄이라 지금은 문제가 없다. 하지만 `\` 뒤에 다른 선언이 오면 그 선언이 앞줄에 합쳐져 사라진다. 발견하기 어려운 종류의 결함이라 지웠다.

## 테스트 콘솔 명령의 Shipping 가드

`DMTestFinishMatch` 등 디버그 Exec 명령이 Shipping 빌드에도 포함됐다. 호스트만 실행할 수 있고 Shipping은 기본적으로 콘솔이 꺼져 있어 위험은 낮았다.

UHT는 `UFUNCTION` 선언을 `#if !UE_BUILD_SHIPPING` 블록 안에 두는 것을 허용하지 않는다. 그래서 **선언은 그대로 두고 본문을 감쌌다.**

```cpp
void ADMMatchPlayerController::DMTestFinishMatch(const int32 WinningBaseSlot)
{
#if !UE_BUILD_SHIPPING
    // ... 기존 본문 ...
#else
    // Shipping 빌드에서는 테스트 명령을 실행하지 않는다.
    (void)WinningBaseSlot;
#endif
}
```

본문 앞에서 `return`하는 방식은 Shipping에서 "도달할 수 없는 코드" 경고가 날 수 있어, 본문 전체를 감싸는 방식을 골랐다.

## 재경기 맵 경로의 PIE 접두사

재경기 Travel 경로를 `InitGame()`의 `MapName`에서 가져오고 있었다. 이 값은 짧은 이름이고, PIE에서는 `UEDPIE_0_` 접두사가 붙는다.

```cpp
// InitGame의 MapName은 짧은 이름(PIE에서는 UEDPIE_ 접두사 포함)이므로
// 현재 월드 패키지의 롱 패키지 이름에서 PIE 접두사를 제거해 재경기 경로로 저장한다.
CurrentMatchMapPackageName = IsValid(InitWorldPackage)
    ? UWorld::RemovePIEPrefix(InitWorldPackage->GetName())
    : MapName;   // 예외 상황에서는 기존 동작 유지
```

## 퀘스트 종료 함수의 중복 호출

연장전 전환 경로 `TryStartOvertimeIntro()`에서 `ShutdownQuestSystem()`이 두 번 호출됐다. 연장전 진입 때 퀘스트 종료는 한 번만 일어나야 하는데, 종료 처리에 정리 로직이나 로그가 붙어 있으면 그대로 두 번 실행된다. 한 번만 호출하도록 정리하고 최신 소스에서 다시 검색해 남은 중복이 없음을 확인했다.

## 도입하지 않은 것: 로그 Wrapper

로그 호출이 많아지면서 별도 Log Header나 단축 매크로를 둘지 검토했다. 단순 Wrapper는 헤더 의존성을 늘리고, 원본 로그 위치를 찾을 때 한 단계를 더 거치게 한다. 지금 필요한 것은 호출을 줄이는 것보다 **실패 위치와 맥락(NetMode, 대상 이름, 사유)을 정확히 남기는 것**이라고 보고, 기존 `UE_LOG`와 카테고리별 로그를 유지했다.

## 배포 단계로 넘긴 값

| 항목 | 현재 | 바꿀 시점 |
|---|---|---|
| `SteamDevAppId=480` | Steam 공용 테스트 AppID | 실제 AppID를 발급받은 뒤 |
| `DMNetworkBuildId = 1` | 세션 검색 호환성 식별값 (AppID와 별개) | 네트워크 호환성이 깨지는 버전을 낼 때마다 올림 |

## 정리

- 숫자 하나도 **"왜 이 값인가"** 가 코드에 드러나야 한다. 의미를 확인하는 과정에서 결함 의심이 오탐이었음을 밝혔고, 의심만으로 동작을 바꾸지 않았다.
- 인코딩·줄바꿈처럼 **내용이 바뀌면 안 되는 작업**은 변환 후 비교로 불변을 확인하고, 기능 수정과 분리한다.
- 엔진 도구의 제약(UHT의 전처리기 처리)을 알고 그 안에서 가능한 방법을 고른다.

---

[Refactoring 목차](README.md)
