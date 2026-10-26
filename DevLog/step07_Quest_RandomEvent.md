# 7단계 — 퀘스트·랜덤 이벤트

## 54. Quest와 Random Event 범위 확정

개인별 Quest 진행과 전체 Random Event를 서로 다른 Director로 구성했다. 동시에 켜질 수 있는 시스템의 조합을 제한해 여러 이벤트가 서로 덮어쓰지 않도록 했다.

**구현 및 판단**

- GameMode에서 두 Director 소유
- GameState에는 공개용 상태만 투영
- PlayerState에는 개인 Quest 진행 정보 보유

**결과**

추가 Tick/중복 Active bool 없이 Timer·Delegate 기반으로 전환한다.

## 55. Quest 데이터와 행동 이벤트 연결

Jump·Sprint·Emote·Pose 네 가지 행동 Quest를 진행할 수 있도록 기존 행동에서 Quest 이벤트를 전달했다.

**구현 및 판단**

- 새 Gameplay Tick 대신 실제 행동 발생 시 Event Bridge
- 대상 플레이어·유효 시간·참여 자격 서버 판정
- DataTable 기준 정의 읽기

**결과**

Quest 성공 판정이 UI 입력 이벤트만으로 발생하지 않도록 검증했다.

## 56. Duration Quest와 진행도 집계

일정 시간 유지해야 하는 동작과 횟수 기반 행동을 구분해 개인 진행도를 관리했다.

**구현 및 판단**

- Sprinting/Pose 상태가 중단되면 유지 시간 계산 종료
- 서버의 시작·종료 시각으로 누적값 확정
- OwnerOnly 개인 정보와 공개 상태 분리

**결과**

매 프레임 UI에 진행률을 보내지 않고 필요 데이터만 갱신하는 방향으로 정리했다.

## 57. Quest Deadline과 실패 판정

Quest 제한 시간, 완료 Rank, 실패 대기 상태를 별도로 관리했다. 0등/미완료 상태가 UI에서 1등으로 잘못 표시되지 않도록 후속 위젯에서 주의하도록 했다.

**구현 및 판단**

- 시한 종료와 사망 처리의 선후 관계 확인
- Rank는 QuestDirector에서 확정
- 실패 상태는 매치 종료 시 정리

**결과**

Quest 자체는 7단계에서 처리했지만 최종 개인 UI는 10단계에 이관했다.

## 58. Quest 보상 연동 계획 보류

초기에는 Quest 성공 보상을 스킬 쿨다운 복구로 연결하는 안이 있었다. 그러나 실패자 페널티와 보상 정책이 정해지지 않아 8-11 작업은 보류했다.

**구현 및 판단**

- 보상 비율을 코드에서 곧바로 확정하지 않음
- Quest 결과와 SkillComponent 간 결합을 느슨하게 유지
- 이후 UI 단계에서는 보상 자체를 폐지하는 정책으로 바뀜

**결과**

현재 정책으로는 Quest 성공 시 쿨다운 보상을 지급하지 않는다.

## 59. Blackout 경고와 라이트 표현

Random Event Blackout의 서버 시작·종료 시각과 로컬 암전 표현을 분리했다. `ADMBlackoutController`가 월드 라이트 표현을 담당한다.

**구현 및 판단**

- 서버가 이벤트 구간 확정
- Client는 알림·조명 상태를 표시
- 라이트 변경 전 상태 복원 경로 확인

**결과**

11단계에서 로컬 시야 조명을 붙이는 과정에 조명 컴포넌트 API 호출 오류(`C2039`)가 있어 함께 정리했다.

## 60. NPC Panic 이벤트 전파

전역 Panic을 Random Event Director가 시작하고 NPC Population Director에 전달하도록 구성했다.

**구현 및 판단**

- NPC별 별도 Event Director를 만들지 않음
- Warning→Active→Cleanup 구간 분리
- 월드 NPC 반응과 Player UI 알림 분리

**결과**

NPC마다 반응 지연을 둬 한 프레임에 이동 요청이 몰리지 않게 했다.

## 61. 입력 반전 상태 적용

Input Reverse 이벤트에서 플레이어 이동 방향만 변환하고 다른 Gameplay 규칙의 원본을 덮어쓰지 않도록 설계했다.

**구현 및 판단**

- 상태 원본은 CharacterStateComponent
- Local Move Input 처리에서 Status 확인
- Event 종료 시 정상 입력 즉시 복귀

**결과**

입력 반전 문구가 UI에서 중복 표시되지 않도록 RandomEventHUD·StatusHUD 간 책임을 후속 단계에서 재검토했다.

## 62. Quest·Event 상호배제와 Overtime Cleanup

Quest와 Random Event가 동시에 시작되려는 경우 우선순위와 취소 경로를 결정했다. Overtime 진입에서도 진행 중인 작업을 정리한다.

**구현 및 판단**

- 같은 서버 Frame 발생 경합 검증
- Shutdown 시 Quest/RandomEvent/Blackout 각각 정리
- Finish/Abort에서도 Timer와 표시 상태 종료

**결과**

`TryStartOvertimeIntro()`에서 중복 `ShutdownQuestSystem()` 호출을 찾아 최종 Source에서 제거한 것을 확인했다.

## 63. 퀘스트 실패 사이렌으로 정책 변경

초기의 개인 경고음 반복 방식 대신, 실패한 캐릭터 주변에서 다른 플레이어도 들을 수 있는 3D 루프 사이렌을 채택했다.

**구현 및 판단**

- Character에 AudioComponent·상태 전환 경로
- Sound/Attenuation 에셋은 에디터에서 연결

**결과**

이후 사이렌 시작을 Multicast 대신 복제 상태로 바꿔, 실패 시점에 멀리 있던 플레이어도 가까워지면 듣도록 했다([퀘스트 사이렌](../Docs/Debugging/Gameplay/02_QuestSiren_Relevancy.md)).

## 64. Quest·Event HUD 10단계 이관

Quest Start/Progress/Timer/Rank와 Event Warning/Active는 7단계 Gameplay UI에 억지로 넣지 않았다.

**구현 및 판단**

- GameState 공개 Projection과 PlayerState 개인 진행값 연결 준비
- 미완료·실패 상태 표시 정책
- 10단계 GameRoot 하위에 HUD 배치

**결과**

UI는 Gameplay 상태를 구독만 하며 Quest 성공/실패를 결정하지 않는다.

## 65. 7단계 Source 최종 점검

퀘스트·이벤트 구조에서 Timer, Delegate, 불필요한 Tick, 같은 상태 중복 저장 여부를 다시 검사했다.

**구현 및 판단**

- Quest Shutdown 중복 호출 제거 확인
- 일부 오래된 주석 문구는 코드 기능과 무관한 정리 과제로 남음
- Skill 시스템이 읽을 결과만 노출

**결과**

8단계로 넘기면서 보상 미적용을 명시적으로 기록했다.
