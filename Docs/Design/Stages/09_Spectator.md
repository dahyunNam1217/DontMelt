# 09 — 사망 후 관전·리스폰 화면

## 1. 30초 관전 순서

| 시간 | 단계 | 처리 |
|---|---|---|
| 0~5초 | Corpse | 사망 위치와 실제 시체를 바라보는 카메라 |
| 5~10초 | KillerView | 공격자의 **현재** Pawn을 ViewTarget으로 |
| 10~25초 | FreeCamera | 벽·바닥을 통과하지 않는 관전 카메라 |
| 25~30초 | Fade | 암전, 5·4·3·2·1 표시 |
| 30초 이후 | Respawn | 서버가 새 Pawn 스폰·Possess 후 기본 조작 복귀 |

합계 `5 + 5 + 15 + 5 = 30초`다. 지난 장면을 녹화하는 Kill Replay는 쓰지 않는다. KillerView도 살아 있는 월드의 현재 모습이다.

## 2. 서버와 로컬 역할

서버가 사망을 확정하고 리스폰 타이머를 시작한다. 소유 클라이언트는 `FDMSpectatorContext`로 사망 위치, 공격자 Pawn, 시작 서버 시각을 받아 카메라·입력·Fade를 처리한다. 지연이 있어도 30초를 처음부터 세지 않고 `GetServerWorldTimeSeconds() − SpectatorStartServerTime`으로 현재 단계를 계산한다.

`LifeState`의 원본은 MatchPlayerState다. `EDMSpectatorPhase`는 소유자 로컬 표현 전용이라 복제하지 않는다.

## 3. Corpse와 KillerView

DeathCam은 사망한 SkeletalMesh의 Bounds를 중심으로 잡고, 없으면 전달받은 사망 위치를 쓴다. 기본 거리 350cm, 높이 120cm, Blend 0.2초다. 카메라 트레이스가 지형에 막히면 벽 앞으로 당긴다.

KillerView는 `SetViewTargetWithBlend()`로 공격자의 현재 Pawn을 본다. 도중에 대상이 죽거나 사라지면 DeathCam으로 대체하고, 5~10초 구간 자체는 늘리지 않는다. 대상 유효성을 확인하려고 다른 플레이어의 LifeState를 관전자에게 공개하지 않는다.

## 4. 자유 관전

`ADMRestrictedSpectatorPawn`은 소유 클라이언트에만 만드는 비복제 Actor다. 일반 Pawn Possess는 바꾸지 않고 ViewTarget으로만 쓴다. Sweep 이동과 충돌로 벽·천장·바닥 통과를 막는다. 관전 중에도 NPC와 플레이어의 정체, 상대 유물 수 같은 숨겨진 정보는 UI에 노출하지 않는다.

## 5. Fade와 Handoff

25~30초는 `PlayerCameraManager::StartCameraFade()`를 쓴다. 실제 리스폰은 Fade나 카운트다운 타이머에서 실행하지 않는다. 서버가 새 Pawn 스폰·Possess·초기 상태 반영에 **성공한 뒤** 클라이언트에 Handoff를 알린다.

Handoff를 받으면 관전 ViewTarget·입력·카메라·Fade·임시 Actor·Delegate를 정리하고, 새 Pawn으로 HUD를 다시 연결한다. 사망으로 경기가 바로 끝난 경우에는 관전을 새로 시작하지 않는다.

## 6. 이후 변경

- 10단계에서 관전 HUD를 GameRoot에 붙였다. 기존 카운트다운 위젯은 이중으로 만들지 않는다.
- 관전 시작 RPC 호출이 빠져 있던 경로를 복구하고, Handoff를 서버 성공 뒤로 옮겼다([Handoff 경합](../../Debugging/Gameplay/08_Respawn_Handoff.md)).
- 관전 연출을 `UDMSpectatorPresentationComponent`로 분리했다([책임 분리](../../Refactoring/01_Match_Responsibility_Split.md)).

관련 문서: [관전 구현](../../Technical/13_Spectator_Respawn.md)

[Stages 목차](README.md)
