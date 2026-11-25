# Gameplay Audio — 발생 권한과 청취 범위

소리가 일어난 사건의 소유자와 실제 재생 주체를 분리한다. 상자·유물의 위치는 월드 재생에 쓰고, 개인 결과와 UI 피드백은 로컬 2D로 재생한다.

## 발생 지점과 재생 경로

| 소리 | 발생과 전달 | 위치와 감쇠 |
|---|---|---|
| 공용 UI 클릭 | `UDMGameInstance`가 연결한 버튼의 클릭에서 `PlayCommonUIClickSound`를 호출한다. 로컬 위젯 이벤트로 재생한다. | `PlaySound2D` |
| 개인 경기 알림 | 등록 성공은 등록한 플레이어의 `ClientPlayRelicDepositSound`를 호출한다. 대상 스킬 성공 시 대상 Controller에 피격음을 요청한다. 퀘스트 시작·기한·완료 알림은 로컬 Controller가 종류별 RunId 중복을 걸러 재생한다. | 수신 측 `PlayLocal2DSound` |
| 랜덤 이벤트 예고 | Controller가 예약된 이벤트 상태를 읽고, 시작 전 `EventTag`와 `StartServerTime`을 기준으로 중복을 거른다. | 로컬 2D |
| 상자 열기·닫기 | 서버 Controller가 열기 이벤트를 발행한다. 닫기는 `ServerCloseChestInteraction`에서 서버에 기록한 상자와 받은 상자가 일치할 때 발행한다. | 상자의 월드 위치와 `GameplayWorldAttenuation` |
| 유물 획득·드롭 | 서버 상호작용 성공 경로에서 유물 또는 상자의 위치를 전달한다. 드롭은 유물 매니저의 확정 위치를 사용한다. | 사건의 월드 위치와 `GameplayWorldAttenuation` |

월드 사건은 권한이 있는 `ADMMatchGameState::BroadcastWorldAudioEvent`가 종류와 양자화 위치를 **Unreliable NetMulticast**로 전한다. 수신 월드의 로컬 Controller가 사건 종류에 해당하는 음원을 골라 `PlaySoundAtLocation`을 호출한다. 전용 서버에는 재생을 만들지 않는다. Multicast는 소리 재생용 사건이며, 지연이나 유실과 관계없이 유물·상자 상태는 각각의 서버 판정 경로에서 관리한다.

[UI 클릭](../../Source/DontMelt/Private/Core/DMGameInstance.cpp) · [월드 사건 복제 선언](../../Source/DontMelt/Public/Framework/Match/DMMatchGameState.h) · [재생 분기](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp) · [획득·등록](../../Source/DontMelt/Private/Characters/Components/DMInteractionComponent.cpp) · [드롭 위치](../../Source/DontMelt/Private/Relic/DMRelicManager.cpp)

## 캐릭터 소리

- **발소리**는 애니메이션 Notify 경로에서 플레이어 캐릭터의 유효한 지상 이동 등을 확인하고 위치 기반으로 재생한다. `FootstepAttenuation`과 로컬 볼륨 배율을 적용한다. NPC 이동 발소리는 이 경로에서 제외한다. 발소리 증폭은 시전자 측 청취 표현만 조절하며, 대상 본인의 경고 HUD는 별도 전달이다.
- **점프**는 로컬 소유자가 점프한 시점에 재생하고, 서버 Multicast가 원격 표현을 보완한다. 소유자는 Multicast 재생에서 제외해 로컬 재생과 겹치지 않게 한다. 위치 기반 재생에 `JumpAttenuation`을 넘긴다.
- **전투와 반응**은 공격·방어 Notify와 스턴·NPC 반응·사망 등의 진입 경로에서 캐릭터 오디오 컴포넌트를 사용한다. 전투에는 `CombatAttenuation`, 반응에는 유효한 `ReactionAttenuation`을 우선 사용하고 없으면 전투 감쇠를 사용한다.

[캐릭터 오디오](../../Source/DontMelt/Private/Characters/Components/DMCharacterAudioComponent.cpp) · [점프의 로컬 및 원격 분기](../../Source/DontMelt/Private/Characters/DMPlayerCharacter.cpp) · [공격·방어 Notify](../../Source/DontMelt/Private/Animations/DMAnimNotify_CombatVFX.cpp)

## 에셋과 청취 검증

`65630c3`에 Gameplay SoundWave와 `ATT_DMNear`, `ATT_DMMedium`, `ATT_DMFar`가 보존돼 있다. 각 소리의 실제 BP 할당, 감쇠 거리·곡선, 볼륨 믹스, Host 중복 재생 및 원격 청취 결과는 미검증이다. Host와 Client에서 상자 근거리·원거리, 개인 등록·퀘스트 알림, 점프 소유자·관찰자를 각각 확인한다. 실패 사이렌 및 Steam 음성은 별도 수명·청취 정책을 따르는 경로다. [Gameplay 오디오 에셋 커밋](https://github.com/dahyunNam1217/DontMelt/commit/65630c3) · [퀘스트 실패 사이렌](../Debugging/Gameplay/02_QuestSiren_Relevancy.md) · [Steam 음성](14_Voice_Chat.md)
