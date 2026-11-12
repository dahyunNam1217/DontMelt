# 10. 사망 관전·서버 리스폰

## 핵심 역할 분리

- `ADMMatchGameMode` — 유효한 사망 Commit 및 Alive/Spectating 등 LifeState 변경 승인.
- `UDMRespawnCoordinator` — 사망 시각 기반 **30초** 서버 타이머, 유효 RespawnPoint 확인, Pawn Spawn/Possess 실패 시 **1초 후 재시도**.
- `UDMSpectatorPresentationComponent` — **소유 클라이언트 로컬** 카메라 단계, Fade, 관전 입력·카운트다운.
- `ADMMatchPlayerController` — `ClientBeginSpectatorSequence(FDMSpectatorContext)`, Handoff RPC 진입 및 HUD/입력 정리.

## 타임라인

| 사망 후 서버 경과 | 카메라/입력 |
|---|---|
| 0~5초 | Corpse: 실제 사망 위치/시체 중심 DeathCam, 벽 차폐 보정 |
| 5~10초 | KillerView: 공격자의 **현재** 시점, 대상이 없으면 Corpse View 대체 |
| 10~25초 | FreeCamera: 제한된 관전 Pawn/ViewTarget, 벽·바닥 Sweep |
| 25~30초 | Fade: 화면 암전 및 **5·4·3·2·1** 표시 |
| 30초 | 서버 리스폰 성공 후 새 Pawn 조작 복귀, **3초 보호** |

관전 시작 지연이 있어도 **로컬에서 30초를 처음부터 재시작하지 않고**, `SpectatorStartServerTime`과 `GetServerWorldTimeSeconds()`의 차이를 계산한다. 녹화 Replay 미지원. 제한 관전 Pawn은 비복제 로컬 Actor이고 Possess를 바꾸지 않고 ViewTarget으로 사용.

## 상태/수명 제약

- 사망 때문에 경기 자체가 Finished/Aborted되면 새 관전 시퀀스를 시작하지 않는다.
- 사망 직후 기존 Gameplay Input Context, 상자/스킬/화면 효과 정리 후 관전 Context 설정.
- 관전 화면에서 상대 정체·유물 수 등 감춰야 할 정보를 공개하지 않는다. 음성 모드는 `DeadGlobal`.
- 서버 Spawn+Possess에 실패하면 LifeState를 Alive로 먼저 바꾸지 않고 기존 Spectating 상태를 유지하며 재시도.
- 새 Pawn/Handoff 성공 뒤 이전 Character·UI Delegate 바인딩 정리, Gameplay 입력·GameOnly/마우스 캡처·생존 근접 Voice 복귀.

## 검증

Client 지연/손실, Killer가 사망/이탈, DeathCam 벽, FreeCamera 충돌, 리스폰 스폰 불가 후 재시도, 30초 카운트다운, 보호 중 상호작용/스킬/공격 차단, 종료 직전 사망/관전 진입 차단, 연속 사망 후 이전 타이머 잔존 여부.

[설계](../../Docs/Design/Stages/09_Spectator.md) · [구현 기록](../../Docs/Technical/13_Spectator_Respawn.md) · [Handoff 경합](../../Docs/Debugging/Gameplay/08_Respawn_Handoff.md)
