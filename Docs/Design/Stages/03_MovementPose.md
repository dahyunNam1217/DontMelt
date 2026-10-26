# 03 — 이동·카메라·자세

## 1. Character 책임

`ADMJellyCharacterBase`가 플레이어와 NPC의 공통 메시·이동·애니메이션 진입점이다. `ADMPlayerCharacter`는 카메라와 입력을 받고, NPC는 `ADMNPCCharacter`와 서버 AIController가 움직인다. 이동 명령을 만드는 주체는 다르지만 실제 이동 처리는 같은 구조를 쓴다.

## 2. 이동과 Sprint 예측

기본 이동은 `CharacterMovementComponent`다. 별도의 이동 Enum을 복제하지 않고 `Velocity`, `MovementMode`, `IsFalling`, Sprint 요청으로 표현 상태를 계산한다. Sprint는 `UDMCharacterMovementComponent`의 SavedMove와 `FLAG_Custom_0`으로 예측·보정에 참여시킨다. 키를 누를 때마다 Sprint RPC를 보내고 bool을 복제하는 구조는 쓰지 않는다.

`PostInitializeComponents()`에서 Blueprint 설정값으로 걷기 속도를 초기화한다. Sprint를 끝낼 때는 속도를 강제로 되돌리지 않고 `ClearSprintRequest()`로 입력 의도만 해제한다. 나머지는 `GetMaxSpeed()`가 계산한다.

## 3. 카메라

| 모드 | 몸 회전 | 카메라 |
|---|---|---|
| 1인칭 | Controller Yaw 사용 | 캐릭터 눈 위치, 자기 Mesh 숨김 |
| 3인칭 | 이동 방향으로 회전 | SpringArm을 통한 독립 시점 |
| 자세 | 몸 방향 고정 | 진입 전 1·3인칭 상태 유지 |

1인칭은 `bUseControllerRotationYaw = true`, `bOrientRotationToMovement = false`, 3인칭은 반대다. 카메라 회전과 몸 회전이 서로를 따라가는 피드백 루프 때문에 대각선 이동 때 캐릭터가 원을 그리며 도는 문제가 있었고, 시점별로 회전 정책을 분리해 해결했다.

## 4. 자세

`UDMPoseComponent`가 자세 상태를 관리한다. 기본 → 납작 → 웅크리기 → 대자 → 기본 순환이다. 자세를 유지한 채 이동·점프할 수 있고, 몸은 진입 당시 방향을 유지한다. 자세 중에는 공격·방어·달리기·스킬·상호작용을 막고, 카메라 전환은 허용한다. 자세 종료 뒤에도 바꾼 시점은 그대로 남는다.

## 5. 입력 전환

평상시는 `IMC_Gameplay`, 자세 중에는 `IMC_Pose`다. `R`로 들어가고 나올 때 눌린 키가 새 Context로 넘어가 한 번 더 처리되지 않도록 `bIgnoreAllPressedKeysUntilRelease`를 쓴다. 상자·스킬·감정표현 Context는 이후 단계에서 같은 방식으로 늘렸다.

## 6. 이후 변경

- NPC 단계 직전에 공통 `WalkSpeed`를 600에서 300으로 낮췄다. 공통 Base 클래스 값이라 NPC Blueprint가 덮어쓰지 않으면 NPC에도 적용된다.
- 11.5단계 젤리 애니메이션은 이동 원본을 바꾸지 않고 본 회전과 스프링만 더했다.
- 경기 시작 후 마우스 버튼을 누르고 있어야 시점이 도는 문제는 로비 입력 모드가 남아 생긴 것으로, [입력 모드 복구](../../Debugging/UI/02_InputMode_Restore.md)에서 해결했다.

관련 문서: [입력 설계](../EnhancedInput.md) · [입력과 이동 구현](../../Technical/05_Input_Movement.md)

[Stages 목차](README.md)
