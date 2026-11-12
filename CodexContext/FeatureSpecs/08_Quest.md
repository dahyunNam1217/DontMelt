# 08. 서버 관측형 퀘스트·실패 사이렌

## 목적/책임

퀘스트는 캐릭터에게 특정 행동을 강요해 NPC 위장 심리전에 변화를 준다. **완료 보상(스킬 쿨다운 감소)은 최종 설계에서 삭제**했다. `UDMQuestDirector`는 전역 일정·퀘스트 Run·마감·순위, `UDMQuestProgressComponent`(PlayerState)는 개인 진행도, GameState는 공개 퀘스트 정보만 담당.

## 퀘스트 종류

| 종류 | 서버 관측 조건 | 초기 테스트 값(최종 에셋 확인 필요) |
|---|---|---|
| Jump | 실제 Jump 이벤트 횟수 | 3회 |
| Sprint | 실제 달리기 누적 시간 | 3초 |
| Emote | 실제 Emote 상태 **연속** 유지 | 3초 |
| Pose | 지정 자세 **연속** 유지 | 3초 |

초기 제한 시간 각 20초. 퀘스트 시도는 Playing 시작 후 **120/360/600초**, 매치에서 2~3회 시도하며 조건 불만족이면 건너뛰고 재시도 대기열을 만들지 않는다.

## 실행 및 종료 상태

1. 서버가 Playing, 참가자, 다른 퀘스트/랜덤 이벤트 없음 등을 검증하고 Run 시작.
2. `FDMQuestActionEvent`가 실제 행동 Delegate에서 발생할 때 `HandleQuestActionEvent`가 Run·LifeState·참가 여부 검증.
3. Duration 형은 목표 시각까지 타이머/연속 상태 추적으로 처리하고 매 Tick에서 입력 누적하지 않는다.
4. 제한 내 완료는 서버 시각 순서로 순위를 정한다.
5. 제한 지나 미완료 → `FailedPendingCompletion`; 3D 사이렌 활성. 실패 뒤 남은 행동을 완료 → `CompletedAfterFailure`, 사이렌 제거.
6. Phase 전환·사망·경기 종료 시 타이머/표시/바인딩 정리(세부 취소 조건은 실제 코드 확인).

## 정보 공개/표현

퀘스트 종류와 마감 시각 등 공통 정보는 GameState, **개인 진행도**는 OwnerOnly. 타인의 진행도·성공 여부가 불필요하게 새지 않도록 한다. 실패 사이렌은 주변의 다른 관찰자에게도 보이는 지속 표현이므로 늦게 Relevant해진 클라이언트에서도 복원 가능한 표현 상태를 둔다. UI는 실제 서버 관측 진행 이벤트만 표시한다.

## 완료 기준

- 입력 스팸/에어본 중 스페이스 반복이 점프로 중복 집계되지 않음.
- Sprint가 실제로 가능한 상태일 때만 누적, Emote/Pose 연속 유지 중 해제/사망하면 정확히 중단.
- 제한 시간 초과 직후 사이렌, 뒤늦은 완료 시 제거, 새로 Relevant해진 관찰자에 동일하게 표시.
- 동시에 랜덤 이벤트와 시작되지 않음, 연장전/결과에서 타이머 해제, 퀘스트 보상 미지급.

[설계](../../Docs/Design/Stages/07_QuestRandomEvent.md) · [구현 기록](../../Docs/Technical/09_Quest_RandomEvent.md) · [사이렌 회귀](../../Docs/Debugging/Gameplay/02_QuestSiren_Relevancy.md)
