# 14. 로컬 설정·Network/Travel 실패 복구

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
