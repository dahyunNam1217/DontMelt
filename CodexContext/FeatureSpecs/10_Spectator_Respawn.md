# 10. 사망 관전·서버 리스폰

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E9-B — 서버 리스폰 재시도 상한

**설계 승인:** 최초 실패 후 최대3회 재시도, 서버 관리, 무한 재시도 방지, 성공 시 Pawn·Controller·PlayerState 연결 복구, 실패 원인·소진 기록을 유지한다.

**구현 확인:** `UDMRespawnCoordinator::HandleRespawnTimerExpired` / `ScheduleRespawnRetry`가 재시도와 연결·보호 적용을 관리한다. 기본 재시도 간격은 GameMode의1초이며 최소0.05초 보정이 있다. 최대3회 소진 시 현재 코드는 전체 경기 내부 실패 중단을 요청한다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E9-B 절을 함께 읽는다.

### E10 — 정보 공개 범주와 Listen Host 신뢰 경계

**설계 승인:** Server Authority·최소 전송·OwnerOnly를 유지한다. 전체 Client 공개 / Owner Client 전용 / 스킬 시전자 전용 / 관전자 예외 / 서버 전용으로 구분한다. 판정 원본과 표현 데이터를 분리한다. UI 비표시를 비전송 보장으로 취급하지 않는다. Listen Host는 권위 서버를 실행하는 신뢰 서버다. Host에게 서버 원본까지 은폐한다고 보장하지 않는다.

| 범주 | 목표 계약 | 현재 코드 관측 / 승인 경계 |
|---|---|---|
| 전체 Client 공개 | 공개 경기 상태·필요한 관찰 표현만 | GameState Phase·공개 이벤트/퀘스트·최종 결과, PlayerState rematch ready, Character Action/스턴·투명 표현. Relevant 범위와 필드별 재검수 필요 |
| Owner 전용 | 개인 상태·개인 응답 최소화 | MatchPlayerState LifeState/BaseSlot/운반 여부, CharacterState 상세 태그·투명화 시간은 OwnerOnly |
| 시전자 전용 | 기능에 필요한 최소 정보의 예외 명세 | ClientSetFootstepAmplifyPresentation은 대상 PlayerState를 전달. 현재 전달 사실이며 해당 참조 전체의 최종 예외 승인은 시스템 명세 확인 필요 |
| 관전자 예외 | 관전 정책상 필요한 범위만 | ClientBeginSpectatorSequence의 Context·KillerPawn 등은 현재 구현. 숨길 정보와 예외 범위를 별도 확인 |
| 서버 전용 | 원본 판정·불필요한 신원/운반 관계 | RelicManager 비복제, 스킬 서버 대상 선정. 클라이언트가 대상 Actor 대신 선택 정책을 제출 |

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E10 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

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
