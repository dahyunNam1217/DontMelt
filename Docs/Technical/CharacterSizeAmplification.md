# 크기 증폭 — 서버 캡슐과 클라이언트 표현

크기 증폭은 대상 Pawn의 외형과 충돌 캡슐을 함께 바꾼다. 서버가 공간 적합성과 캡슐 전환을 판정하고, 클라이언트는 서버 시간에 맞춰 표현을 갱신한다.

## 요청과 서버 판정

1. `ADMPlayerCharacter::BindSkillInput`이 크기 증폭 Action을 연결한다. `ADMMatchPlayerController`는 대상 선택 진입을 서버에 요청하고, 선택 정책을 받은 서버가 대상 자격을 해석한다.
2. `UDMSkillComponent::TryExecuteSizeAmplification`은 권한, 스킬 가능 여부, 대상 자격, 정의의 지속시간 및 대상 Pawn의 컴포넌트를 확인한다. `TryStartSizeAmplification`은 중복 상태와 시간·배율 조건을 검사하고, 최종 목표 캡슐을 미리 검사한 뒤 `Status_SizeAmplified`를 적용한다.
3. 효과 시작에 성공하면 스킬 쿨다운을 확정한다. 쿨다운 확정에 실패하면 `StopSizeAmplification(true)`로 효과를 정리하고, 성공하면 대상 Controller에 피격 사운드를 요청한다. 이후 전환 도중 새 충돌로 효과가 중단되는 경로는 이미 확정한 쿨다운의 반환을 보장하지 않는다.

지속시간은 스킬 정의에서 읽고, 배율·전환 시간은 스킬 컴포넌트에서 전달한다. 활성 DataAsset과 BP 설정 및 최종 밸런스는 별도 확인 범위다. [서버 대상 선택](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp) · [효과 실행](../../Source/DontMelt/Private/Skills/DMSkillComponent.cpp)

## 캡슐 전환과 실패 롤백

`CacheOriginalValues`는 캡슐의 원래 반경·반높이와 표현의 기준값을 보관한다. `UpdateSizeTransition`은 서버 시각에서 확대·유지·축소 구간의 배율을 계산한다. 전환 중에는 타이머를 갱신하고, 최대 크기 유지 중에는 축소 시작만 예약한다.

`TryApplyCapsuleDimensions`는 반높이 변화량만큼 캡슐 중심을 수직으로 이동해 하단 높이를 유지하려 한다. `CanUseCapsuleSize`는 목적 위치에서 목표 캡슐의 블로킹 겹침을 검사한다. 축소할 때는 기존 큰 캡슐이 바닥에 걸리지 않도록 먼저 크기를 줄이고, 확대할 때는 중심 이동 성공 후 크기를 적용한다.

Sweep이 막히거나 목표 위치에 도달하지 못하면 `RollbackCapsuleTransition`이 이전 위치·회전·크기로 되돌리는 경로를 실행한다. 원복 중 허용된 폴백은 롤백 성공과 목표 형상의 재검사를 조건으로 한다. 일반 확대 중 해당 실패가 나면 서버가 효과를 중단한다. [캡슐과 겹침 검사](../../Source/DontMelt/Private/Characters/Components/DMSizeAmplificationComponent.cpp)

## 원복·복제·카메라

| 항목 | 구현 경계 |
|---|---|
| `SizeState` | `StartServerTime`, `EndServerTime`, `TargetScale`, `TransitionDuration`을 복제한다. `OnRep_SizeState`가 현재 서버 시각에 맞춰 표현을 시작하거나 원복한다. |
| 소유자 타이밍 | `UDMCharacterStateComponent`의 `SizeAmplificationTiming`은 owner-only다. 상태 HUD의 시간 정보와 외형에 필요한 복제 상태를 분리한다. |
| 카메라·이동 예측 | 크기 적용은 캐릭터의 카메라 스케일 경로와 연결된다. `ResetAutonomousProxyPrediction`은 Autonomous Proxy에서 크기 변경 통지와 원복 뒤 예측 데이터를 초기화한다. 서버 캡슐 적합성은 서버가 결정한다. |
| 종료 | 상태 제거·만료 등에서 `StopSizeAmplification`이 타이머, 상태, 크기와 표현을 정리한다. 빈 복제 상태와 원복 Multicast가 클라이언트 경로에 전달된다. `EndPlay`도 타이머와 상태 구독을 정리한다. |

[복제 상태 선언](../../Source/DontMelt/Public/Characters/Components/DMSizeAmplificationComponent.h) · [소유자 상태](../../Source/DontMelt/Private/Characters/Components/DMCharacterStateComponent.cpp) · [카메라 적용](../../Source/DontMelt/Private/Characters/DMPlayerCharacter.cpp)

## Host와 Client에서 확인할 항목

좁은 공간에서 시작 거부 여부, 확대 중 이동·충돌로 인한 중단, 축소·원복 때 바닥과 위치, 늦게 Relevant해진 관찰자의 외형, 소유자 카메라와 이동 보정을 각각 확인한다. 사망·리스폰 이후 상태와 캡슐도 점검한다. 이 체크포인트의 PIE·Steam 재현과 BP 적용값은 미검증이다. [스킬 공통 흐름](10_Skill_Cooldown_Target.md) · [입력과 이동 예측](05_Input_Movement.md) · [회귀 시나리오](../Verification/03_Regression_Scenarios.md)
