# Stages — 단계별 설계

기능을 한 번에 만들지 않고 단계별로 설계 → 구현 → 검수를 반복했다. 각 문서는 그 단계에서 확정한 규칙과 시스템 경계, 그리고 이후 단계에서 바뀐 점을 담는다. 최종 규칙은 [게임 기획서](../GameDesign.md)를 기준으로 한다.

| 단계 | 문서 | 핵심 |
|---|---|---|
| 0 | [게임 콘셉트와 규칙 범위](00_GameConcept.md) | 정체를 숨기는 심리전, 처음부터 고정한 제약 |
| 1 | [기본 클래스와 데이터 소유권](01_CoreArchitecture.md) | Framework 분리, 설계 원칙 |
| 2 | [로비·경기 Framework](02_MatchFramework.md) | Ready·시작 검증, Phase 전환, 참가자 확정 |
| 3 | [이동·카메라·자세](03_MovementPose.md) | Sprint 예측, 시점별 회전 정책, 자세 |
| 4 | [유물·상호작용](04_RelicInteraction.md) | 유물 상태 전이, 예약, 서버 재검증 |
| 5 | [전투·사망·리스폰](05_CombatDeathRespawn.md) | Notify 시점 판정, 방어, 사망 확정 순서 |
| 6 | [NPC 인구 관리와 AI 행동](06_NPCAI.md) | 서버 전용 타이머 상태 머신, 집단 반응 |
| 7 | [퀘스트·랜덤 이벤트](07_QuestRandomEvent.md) | 전역 일정과 개인 진행 분리, 실패 사이렌 |
| 8 | [스킬 시스템](08_Skills.md) | PlayerState 소유 쿨다운, 서버 대상 선택 |
| 9 | [사망 후 관전·리스폰 화면](09_Spectator.md) | 30초 관전 단계, 서버 성공 후 Handoff |
| 10 | [UI·결과·설정 통합](10_UIAndMatchResult.md) | Root Widget, 결과 ACK, 설정 확인 흐름 |
| 11 | [Steam 세션·음성 통합](11_SteamSessionVoice.md) | 단계별 접속 문제, 생존 상태별 청취 |
| 11.5 | [젤리 관성 애니메이션·사망 Ragdoll](11_5_JellyAnimationAndRagdoll.md) | 감쇠 스프링 7개, AnimGraph, 원격 스턴 표현 |

[Design 목차](../README.md)
