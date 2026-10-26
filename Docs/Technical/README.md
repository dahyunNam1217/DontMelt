# Technical — C++ 구현

UE5.5 Listen Server 환경에서 시스템별로 **누가 상태의 원본인가, 어떤 경로로 전달되는가, 실패하면 어떻게 되돌리는가**를 중심으로 정리했다. 각 문서 끝에 관련 소스 파일과 함수를 적었다.

## 먼저 볼 문서

| 문서 | 핵심 |
|---|---|
| [01. 아키텍처](01_Architecture.md) | 서버 상태와 클라이언트 표현 분리, 클래스별 책임 |
| [04. 복제와 RPC 기준](04_Replication_RPC.md) | OwnerOnly 복제, 사건(RPC)과 상태(RepNotify) 구분 |
| [06. 유물 상호작용](06_Relic_Interaction.md) | RequestId, 예약, 진행 중 재검증, Commit 직전 재확인 |
| [07. 전투](07_Combat_Death.md) | 예측 몽타주와 서버 히트 판정, 사망 확정 경계 |
| [12. 젤리 애니메이션](12_Jelly_Animation_Ragdoll.md) | 감쇠 스프링 적분, 서브스텝, 사망 시에만 Ragdoll |

## 전체 목록

| 번호 | 문서 | 핵심 |
|---:|---|---|
| 01 | [아키텍처](01_Architecture.md) | 상태 원본과 데이터 흐름 |
| 02 | [Steam 세션과 로비](02_Steam_Session_Lobby.md) | 비동기 세션 작업, 실패 복구, 로비 시작 검증 |
| 03 | [경기 흐름과 결과](03_Match_Flow_Result.md) | Phase 전환 규칙, 승패 판정 순수 함수, 결과 ACK |
| 04 | [복제와 RPC 기준](04_Replication_RPC.md) | 복제 범위, RPC 종류별 용도 |
| 05 | [입력과 이동](05_Input_Movement.md) | Mapping Context 전환, Sprint를 SavedMove 플래그로 |
| 06 | [유물 상호작용](06_Relic_Interaction.md) | 서버 검증형 트랜잭션과 예약 |
| 07 | [전투](07_Combat_Death.md) | 예측, 히트 판정, 사망 |
| 08 | [NPC](08_NPC_AI.md) | 인구 확정, 타이머 기반 AI, 집단 반응 |
| 09 | [퀘스트와 랜덤 이벤트](09_Quest_RandomEvent.md) | 서버 일정과 개인 진행도 분리 |
| 10 | [스킬](10_Skill_Cooldown_Target.md) | 서버가 고르는 대상, 종료 시각 복제 |
| 11 | [UI](11_UMG_UI.md) | Root Widget 레이어, 상태 기반 갱신 |
| 12 | [젤리 애니메이션](12_Jelly_Animation_Ragdoll.md) | 절차적 관성 표현, 사망 Ragdoll |
| 13 | [사망 관전](13_Spectator_Respawn.md) | 서버 생명주기와 로컬 카메라 단계 |
| 14 | [음성 채팅](14_Voice_Chat.md) | Steam Voice, 생존 상태별 청취 정책 |
| 15 | [설정과 장애 복구](15_Settings_NetworkRecovery.md) | 로컬 설정 소유자, 실패 상황별 복구 |

소스 경로는 저장소 루트의 `Source/DontMelt/`를 기준으로 한다.

[Docs 목차](../README.md)
