# FeatureSpecs — 기능별 최종 설계

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

기능별 목표 승인과 구현 현황의 상세는 중앙10절을 사용한다. 16번 신규 시각 표현 등 이번 E1~E10 승인 밖의 제안은 기존 상태를 유지한다.

**이 폴더의 모든 문서는 목표 동작과 검증 조건을 정의한다.** '설계 확정'과 '현재 코드 구현 확인'은 다르다. 구현 현황은 [ImplementationStatus](../ImplementationStatus.md)에서 별도 관리한다.

| 순서 | 기능 명세 | 핵심 검증 |
|---|---|---|
| 01 | [Steam 세션·로비](01_Session_Lobby.md) | 비동기 성공/실패, Host Ready·Travel 잠금 |
| 02 | [경기 흐름·결과](02_Match_Lifecycle_Result.md) | Phase, 종료·순위·ResultId ACK |
| 03 | [이동·입력·자세](03_Character_Input_Movement_Pose.md) | SavedMove Sprint, 모드 전환·Pawn 교체 |
| 04 | [유물·상자·상호작용](04_Relic_Interaction.md) | RequestId/예약/Commit/정보 비공개 |
| 05 | [전투·방어·사망](05_Combat_Death.md) | Notify 히트, 오인 공격, 보호·Ragdoll |
| 06 | [NPC 군중·AI](06_NPC_AI.md) | 배율·스폰·타이머·군중 반응 |
| 07 | [스킬](07_Skills.md) | 서버 대상 선정·쿨다운·표현 |
| 08 | [퀘스트](08_Quest.md) | 실동작 관측·시간·실패 사이렌 |
| 09 | [랜덤 이벤트](09_Random_Events.md) | 조건 중복 배제·정전/패닉/반전 |
| 10 | [관전·리스폰](10_Spectator_Respawn.md) | 서버 리스폰·로컬 카메라·Handoff |
| 11 | [UMG·결과 UI](11_UI_UX.md) | Root 레이어·상태 알림·정보 제한 |
| 12 | [Steam 음성](12_Voice_Chat.md) | 생존 근접/사망 전체 청취·PTT |
| 13 | [젤리 애니메이션·Ragdoll](13_Jelly_Animation_Ragdoll.md) | 로컬 본 스프링·사망 물리·성능 |
| 14 | [설정·연결 장애 복구](14_Settings_Network_Recovery.md) | 로컬 설정·실패 복귀·세션 정리 |
| 15 | [맵·스폰·에셋](15_Map_Spawn_Assets.md) | BaseSlot·스폰 지점·에셋 계약 |
| 16 | [시각 이펙트·부착형 메시](16_Visual_Effects_And_Presentation.md) | C++ 판정/BP 연출 분리, 최신 요구 미검증 |

## 문서 독해 규칙

- **확정 규칙**: 기존 최종 기획·설계 문서에 명시.
- **아키텍처 계약**: 기존 시스템의 데이터 소유권·호출/정리 원칙.
- **검증 필요**: 실제 Source/Content/Config가 없거나 구현 일치 여부 확인이 필요한 항목.
- **신규 연출 제안**: 기존 코드에 구현됐다고 추정해서는 안 됨(16번).

각 명세의 C++ 클래스·함수는 원본 기술 문서가 연결한 **코드 검색 후보**이다. 존재하지 않으면 유사 파일과 최신 리네임을 조사한 후 보고한다.
