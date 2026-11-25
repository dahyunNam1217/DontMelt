# 09. 랜덤 이벤트 — 정전·패닉·입력 반전

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](../ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 아래 최신 절과 충돌하는 P2/기존 본문은 당시 설계·조사 이력으로 보존하며 현재 완료 근거로 사용하지 않는다.

- `Public/UI/DMRandomEventHUDWidget.h`는 `TXT_EventDescription`만 BindWidget 필수다. `TXT_EventName`·`PB_EventProgress`는 BindWidgetOptional 호환 필드이며 존재하면 Collapsed 처리한다.
- `Private/UI/DMRandomEventHUDWidget.cpp::RefreshPresentation`은 설명·예고 접두사만 표시하며 이름/진행률 갱신을 제거했다. GameState 공개 이벤트와 DataAsset 설명을 사용하고 정의 누락 시 대체 문구를 표시한다.
- 기존0.1초 로컬 Timer는 예고→진행 전환과 종료 숨김을 위해 유지한다. 비활성·만료·Destruct 시 정리 경로가 있다. 서버 이벤트 판정과 복제 계약을 변경한 작업이 아니다.
- 설명만 있는 WBP 컴파일, 경고→진행→종료, 정의 누락, Host/Client·Travel 재생성은 미검증이다.

## P2 승인 동기화 — 2026-11-13 (당시 기록; 최신 기준은 상단 P3-9)

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E7 — RandomEvent HUD 설명 단일화

**설계 승인:** `TXT_EventDescription`만 표시한다. 이름과 진행바는 표시하지 않으며 필수 UI 바인딩에서도 해제하는 것이 목표다. 설명 갱신, 경고·진행·종료 전환, GameState 연결, 종료 정리는 유지한다. DataAsset DisplayName과 서버 이벤트 판정 데이터는 HUD 미사용을 이유로 제거하지 않는다.

**구현 확인 / 부분 구현:** `UDMRandomEventHUDWidget::RefreshPresentation`은 현재 TXT_EventName/TXT_EventDescription/PB_EventProgress 세 요소를 모두 요구하고 이름·진행률도 갱신한다. `UDMGameRootWidget`은 GameState 및 표시 활성 상태를 연결한다. 조사한 C++에서 외부 시스템이 이름·진행바 자식 포인터를 직접 사용하는 경로는 확인하지 못했다. 기존 세 요소 명세는 이전 버전이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E7 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## 권한과 조건

`UDMRandomEventDirector`가 서버에서 시도 일정·랜덤 선택·중복/동시성 배제·Effect 시작·종료를 확정한다. `ADMMatchGameState`가 공개 이벤트 이름/설명/종료 시각을 전달한다. 시각효과는 `ADMBlackoutController` 등 Client의 로컬 표현이 담당.

## 일정

최종 기획서는 Playing 이후 **240/480/720초**에 시도하고 한 경기에 **2~3회** 실행 시도. 조건이 맞지 않으면 그 차례는 건너뜀. 같은 이벤트 연속 선택 금지. 예고 시작과 실제 효과 시작은 서로 다른 검사 지점: **효과 시작 시각에 Playing/퀘스트 없음/필요한 생존 조건 재검증**. 퀘스트와 동시에 실행 금지.

## 이벤트 사양

| 이벤트 | 예고·지속 | 효과와 범위 |
|---|---|---|
| `Blackout` 젤리나라 정전 | 3초 경고, 2초 전 조명 깜빡임, 효과 10초 | 월드·로컬 시야 조명과 NPC 재질을 변경, 이동·상호작용·전투·스킬·음성 유지 |
| `Panic` 젤리 패닉 | 효과 10초 | 서버 NPC Director에 전달. NPC가 1~3초 간격으로 도주/점프/비명 등 반응 |
| `InputReverse` 젤리 전염병 | 효과 10초 | 살아 있는 플레이어 **WASD 이동 방향만** 반전, Look/UI/점프/공격 등은 정상 |

## 핵심 경계

정전 표현은 서버 이벤트 시간에 맞춰 NPC 재질을 페이드한다. 스폰 이벤트로 새 NPC에 적용하고 종료 시 원상복구한다. 로컬 Controller는 예약 이벤트의 시작 전에 `EventTag`와 `StartServerTime`으로 경고음 중복을 제거한다. 재질과 경고음의 실제 표현은 검증 대기다.

- 정전 중에도 서버 판정용 위치·LOS 규칙을 임의로 변경하지 않는다.
- 입력 반전은 캐릭터의 게임패드나 카메라까지 무조건 반전하는 것이 아님. 입력 소스 범위는 실제 설정에 맞춰 테스트.
- 이벤트 시작·종료로 이미 진행 중인 유물 Hold를 자동 실패 처리하지 않는다. 단 기존 상호작용 고유 유효성 검사는 유지.
- 연장전 소개·Finished·Aborted에서는 이벤트 타이머/상태/입력 반전·조명 복구를 명시적으로 정리.
- 퀘스트/이벤트 동시 발생, 경고 시작 직후 플레이어 사망, Phase 변경과 실제 발동 동시성 검사.

## 완료 기준

3종 각 Host/Client 동시 효과, 중복·연속 이벤트 배제, 예고 중 조건 변동 취소, 원상복귀/월드 이동 정리, 입력 반전이 카메라·UI에 영향 없음, 정전에서 게임 규칙의 다른 판정 일관성.

[기획](../../Docs/Design/GameDesign.md) · [단계 설계](../../Docs/Design/Stages/07_QuestRandomEvent.md) · [기술 기록](../../Docs/Technical/09_Quest_RandomEvent.md)
