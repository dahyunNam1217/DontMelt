# 14. 로컬 설정·Network/Travel 실패 복구

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](../ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 아래 최신 절과 충돌하는 P2/기존 본문은 당시 설계·조사 이력으로 보존하며 현재 완료 근거로 사용하지 않는다.

- `Private/Framework/Lobby/DMLobbyGameMode.cpp`의 `HandleStartMatchRequest`, `TryReopenReturningLobby`, `RestoreLobbyStatus`, `HandleReturningPlayersTimeout`에서 세션 복구 차단을 확인한다.
- `HandleLobbyStatusUpdated`는 유효 구독·진행 문맥·세션 유효성·복구 차단·Busy를 검사한다. 등록 전 기존 Delegate를 해제하고 요청 직후 Handle 유효성도 확인하여 동기 완료의 중복 처리를 방지하는 코드다.
- `StopLobbyStatusRecovery`는 소유 완료 Delegate와 복구/복귀 Timer를 정리하고 `bTravelCommitted=true`로 시작 잠금을 유지한다. `RetryOrReleaseLobbyStatusRestore`라는 기존 이름과 달리 상한 소진 시 잠금을 해제하지 않는다. 상한 상수는5이며 실패 카운트가5에 도달하면 중지한다. 반환 로비 재시도5초/일반 복구1초는 현재 구현값이다.
- `ReleaseMatchStartLock` 호출은 광고 복원 성공 경로에 있다. 종료 시 `EndPlay` 정리도 유지한다. Busy/응답 대기5초 확인 분기까지 모든 반복이 동일 실패 카운트로 제한된다고 단정하지 않는다.
- 기존 OnlineSessionSubsystem 상태 머신을 유지한다. 로그의 나가기·게임/Steam 재시작 안내는 확인했으나 UI 전달과 실제 Steam 정상/실패/지연/중복 콜백은 미검증이다.

## P2 승인 동기화 — 2026-11-13 (당시 기록; 최신 기준은 상단 P3-9)

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E8 — Steam 안전 복구 상태 머신 유지

**설계 승인:** 기존 상태 머신을 유지하며 전면 재작성하지 않는다. Create/Find/Join/Destroy/Update별 비동기 결과를 구분하고 기본 요청30초를 유지한다. 타임아웃은 플랫폼 취소 완료가 아니다. 작업 중 중복 요청을 거절하고 원 요청 완료·필요한 잔여 세션 정리·안전한 재시도 조건 확인 후 다음 요청을 허용한다. 불확실하면 차단·재시작 안내한다. 영구 차단 후 무의미한 로비 재시도를 반복하지 않는다.

**구현 확인:** `UDMOnlineSessionSubsystem::HandleOperationTimeout`은 지연 완료를 기다리고 Find 취소를 시도한다. Create/Join 지연·실패는 `BeginRecoveryDestroy`로 잔여 세션을 처리한다. Destroy 완료는 성공 플래그와 NamedSession 잔존을 확인한다. Find는 취소/완료 결과에 따라 종료하며 Update 지연 완료는 실패 처리한다. `CompleteOperation`은 Delegate·Timer·상태를 정리한 뒤 원 작업 결과를 알린다. `FailClosedRecovery`의 bRecoveryBlocked와 `RejectBlockedOperation`은 불확실한 상태에서 새 요청을 막는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E8 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## 설정 분리

| 데이터 | 소유자·저장 |
|---|---|
| 해상도/창모드/그래픽 | `UGameUserSettings` / 엔진 설정 |
| 마스터·SFX·음악 볼륨 | `UDMLocalSettingsSubsystem`, SaveGame, 값 0.0~1.0 |
| 마우스 감도 | 동일 서브시스템, 값 0.1~3.0 |
| 음성 PTT/음성감지·음소거 | `UDMVoiceChatSubsystem`, 로컬 설정 |

타이틀/로비/인게임에서 동일 Settings Widget(탭 Graphics/Audio/Controls/Language/Voice). **현재 화면 입력값과 마지막 저장값**을 비교하여 변경 유무를 판단; 원래대로 돌려놓으면 변경 없는 것으로 간주. 저장 안 한 채 탭 이동/닫기 시 `적용 / 변경 취소 / 계속 수정`. 해상도·창모드 변경은 **15초 확인 타이머**, 미응답 시 이전 값 복구. 로컬 설정을 PlayerState로 복제하지 않는다.

## 접속 실패 분류

`UDMGameInstance`가 NetworkFailure/TravelFailure를 수신하되 **의도한 나가기**, **ServerTravel 중 기존 연결 종료**, **이미 경기 월드에서 끊김**, **PendingNetGame 연결 중 오류**, **맵 Travel 실패**를 구분.

| 사례 | 기대 처리 |
|---|---|
| IntentionalDisconnect | 연결 오류 팝업 금지, 일반 나가기 처리 |
| TravelURL/PendingNetGame 존재하는 정상 이동 | 기존 연결 종료를 접속 실패로 오인 금지 |
| 실제 Network/Travel 실패 | 중복 복구 차단 → `UDMConnectionLostWidget` 안내 → 메인 메뉴 복귀·입력 복원·잔여 세션 정리 |
| 안내 Widget 생성 불가 | 팝업 없이도 메인 메뉴 복구 진행 |
| 방 나가기 중 DestroySession 응답 지연 | 최대 **8초** 기다린 후 정리 진행 |
| 세션 자체 callback 지연 | 작업별 **30초** 타임아웃 분리 |

**중요:** 연결 중인 월드의 `NetDriver`와 예외 이벤트가 같은 Driver인지 확인하고, Beacon 등 별도 Driver 오류를 전체 게임 실패로 오인하지 않는다.

## 수명/검증

- 실패 복구가 중복 호출되어 Widget·Travel 2회 실행되지 않는지.
- 접속 실패 직후 새 Create/Join을 시도할 수 있게 stale 세션을 정리했는지.
- 로비·경기 Travel 직후 GameOnly 입력 모드/마우스 캡처 정상인지.
- 화면 설정 15초 미응답 자동 복귀, 탭 간 변경 확인, 변경 후 원래 값 복원 시 경고 없음.
- Host/Client 각 NetworkFailure, Host 종료, 잘못된 맵·패키징 누락, Pending 연결 실패.

[기술 문서](../../Docs/Technical/15_Settings_NetworkRecovery.md) · [설정 변경 문제](../../Docs/Debugging/UI/01_Settings_Unsaved_Changes.md) · [Pending 문제](../../Docs/Debugging/Network/06_PendingNetGame_Failure.md)
