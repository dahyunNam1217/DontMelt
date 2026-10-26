# 감정표현 퀘스트를 아무도 완료할 수 없던 문제

| 항목 | 내용 |
|---|---|
| 문제 | 감정표현 유지 퀘스트(`Quest.EmoteDuration`)가 선택되면 모든 플레이어가 완료하지 못했다 |
| 원인 | 플레이어가 쓸 수 있는 Emote 진입점은 연출만 재생했다. 퀘스트가 기다리는 지속형 ActionState 경로는 C++·Blueprint·입력 어디에서도 호출되지 않았다 |
| 해결 | 캐릭터 기반 클래스에 지속형 Emote 진입점을 추가하고, Enhanced Input의 Started / Completed·Canceled에 연결했다 |
| 관련 코드 | `ADMJellyCharacterBase::RequestBeginEmote`, `ADMPlayerCharacter::InputEmoteStarted`, `UDMCharacterStateComponent` |

## 원인 추적

서버 퀘스트는 `QuestEvent.Emote.Start/End`를 기다린다. 이 이벤트가 나오는 경로를 거꾸로 추적했다.

| 경로 | 호출 체인 | 퀘스트 이벤트 |
|---|---|---|
| 일회성 연출 | `ADMJellyCharacterBase::RequestEmote()` (BlueprintCallable) → `StateComponent::RequestEmote` (Server RPC) → `MulticastPresentEmote()` | 없음 |
| 지속형 상태 | `StateComponent::RequestBeginEmote` (Server RPC) → `TryBeginAction(Emote)` → `AuthoritativeEmoteStateChanged` → `ADMPlayerCharacter::HandleAuthoritativeEmoteStateChanged` → `QuestEvent.Emote.Start` | 있음 |

지속형 경로는 세 겹으로 끊겨 있었다.

1. `RequestBeginEmote`와 `RequestEndEmote`를 **호출하는 C++ 코드가 없었다.**
2. 두 함수는 `UFUNCTION(Server, Reliable)`일 뿐 `BlueprintCallable`이 아니어서 **Blueprint에서도 부를 수 없었다.** "BP에서 호출하고 있을 수도 있다"는 가능성이 코드 수준에서 배제된다.
3. 입력 설정 `UDMInputConfigDataAsset`에 Emote 액션이 없었다.

`UDMQuestDirector`는 EmoteId를 구분하지 않고 Start/End 태그만 보므로, 어떤 ID로든 지속형 경로만 타면 진행된다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. `RequestEmote`를 지속형으로 변경 | 기존 함수가 ActionState까지 바꾸게 함 | NPC의 일회성 연출(Scream)도 같은 함수를 쓴다. NPC가 Emote ActionState에 들어가 AI 행동에 영향을 줄 수 있다 |
| B. 퀘스트용 상태 변수 추가 | 연출 시 별도 플래그로 진행 | 서버 ActionState라는 단일 원본이 이미 있는데 두 번째 원본을 만드는 셈이다 |
| **C. 지속형 진입점과 입력 추가** | 기반 클래스에 BlueprintCallable 래퍼, 플레이어 입력 연결 | **채택.** 기존 서버 검증(`CanRequestEmote`)과 상태 전이를 그대로 쓰고, NPC 경로는 건드리지 않는다 |

```cpp
// DMJellyCharacterBase.h
UFUNCTION(BlueprintCallable, Category = "DontMelt|Emote")
void RequestBeginEmote(FName EmoteId);

UFUNCTION(BlueprintCallable, Category = "DontMelt|Emote")
void RequestEndEmote();
```

```cpp
// DMPlayerCharacter.cpp - 누르고 있는 동안 유지, 떼거나 취소되면 종료
EnhancedInputComponent->BindAction(InputConfig->EmoteAction,
    ETriggerEvent::Started, this, &ThisClass::InputEmoteStarted);
EnhancedInputComponent->BindAction(InputConfig->EmoteAction,
    ETriggerEvent::Completed, this, &ThisClass::InputEmoteCompleted);
EnhancedInputComponent->BindAction(InputConfig->EmoteAction,
    ETriggerEvent::Canceled, this, &ThisClass::InputEmoteCompleted);
```

실제 ActionState 허용 여부는 서버가 판정한다. 로컬 `bIsEmoteInputHeld`는 버튼을 눌렀는지만 기록한다. 사망·피격 등으로 서버가 Emote를 먼저 끝냈다면, 이후의 키 해제 요청은 무시된다.

C++ 경로에 맞춰 에디터에서는 `IA_Emote`를 만들어 IMC와 `UDMInputConfigDataAsset::EmoteAction`에 등록하고, AnimBP의 `DefaultEmoteId`를 연결한다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| Emote 키를 3초 유지 | 퀘스트 완료 |
| 2초 유지 후 해제 | 미완료, 다시 누르면 처음부터 |
| 유지 중 사망 | 서버가 Emote 종료, 키 해제 요청 무시 |
| NPC Scream | 기존과 같음 |
| Host와 Client의 진행도 | 일치 |

## 정리

- 서버 이벤트를 기다리는 시스템은 **그 이벤트를 만드는 입력 경로가 끝까지 이어지는지** 거꾸로 따라가 확인한다.
- `Server` RPC와 `BlueprintCallable`은 별개다. RPC가 있다고 BP에서 부를 수 있는 것은 아니다.
- 같은 이름의 기능을 다른 주체(NPC)가 쓰고 있으면, 그 함수를 바꾸지 않고 새 진입점을 만든다.

---

[Debugging 목차](../README.md)
