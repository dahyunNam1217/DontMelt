# 8단계 — 스킬

## 66. Skill 시스템과 기존 상태 충돌 점검

Skill이 Combat·CharacterState·Quest와 별개로 같은 상태를 다시 저장하지 않도록 먼저 검사했다.

**구현 및 판단**

- 스킬 쿨다운은 PlayerState 수명으로 유지
- Pawn에는 일시적 효과만 부여
- 대상을 Client가 직접 확정하지 않음

**결과**

사망·리스폰 후에도 이어지는 값과 Pawn 파괴 시 없어져야 하는 값을 나눴다.

## 67. Skill 정의와 DataAsset 구성

최종 구현 대상은 Invisibility, Footstep Amplify, Vision Distort 세 종류로 정리했다. DataAsset에 지속 시간·쿨다운·효과 정의를 보관한다.

**구현 및 판단**

- GameplayTag를 효과 구분에 사용
- 스킬 수치의 원본은 데이터에 집중

**결과**

초기 기획의 덫·입력 반전 스킬과 Stage 8 구현된 세 스킬은 구분한다. 입력 반전은 별도 Random Event에도 존재한다.

## 68. SkillComponent PlayerState 부착

`UDMSkillComponent`를 MatchPlayerState에 붙여 사망 후 Pawn이 교체되어도 쿨다운 상태를 유지하게 했다.

**구현 및 판단**

- PlayerState가 SkillComponent 수명 관리
- Character와 Controller는 실행 요청/표현 담당
- 불필요한 복제 데이터 생성 억제

**결과**

재접속·매치 종료 시 어떤 데이터가 초기화되는지 분리해서 검토했다.

## 69. 서버 시각 기반 Cooldown

쿨다운 남은 초를 매초 RPC로 보내지 않고 서버 종료 시각에서 계산하도록 했다.

**구현 및 판단**

- 사용 시점에 서버 Availability 검사
- 서버 기준 만료 시각 보유
- Client UI는 계산용 시간과 변경 이벤트를 받음

**결과**

사망/관전 중에도 Cooldown이 흐르며 리스폰 시 새 UI에 다시 바인딩한다.

## 70. 랜덤 대상·유물 리더 대상 선택

클라이언트의 선택은 랜덤 타겟/리더 타겟이라는 의도로 한정했다. 실제 대상 후보 조회와 결정은 서버에서 수행한다.

**구현 및 판단**

- 좌클릭은 랜덤 대상, 우클릭은 등록 유물 최다 보유 대상 방향
- 숨겨야 할 대상 정보는 임의로 공개하지 않음
- 무효 후보·동률·사망 상태 처리

**결과**

클라이언트가 대상 Actor를 그대로 보내면 정보 유출과 조작 가능성이 생기는 구조를 피했다.

## 71. SkillTargeting 입력 Context

스킬 사용 시작과 대상 선택 구간을 별도 Mapping Context로 제어했다. 취소하면 기존 Gameplay 입력으로 복귀한다.

**구현 및 판단**

- `IMC_SkillTargeting` 전환
- 선택 중 다른 행동 제한
- Esc·취소·사망·UI 모달 경합 점검

**결과**

컨텍스트 복원 실패는 게임 입력 전체를 막을 수 있으므로 10단계 UI 검사 항목에 포함했다.

## 72. Invisibility 적용과 Attack Break

투명화 스킬의 적용 시점과 공격 시 해제 규칙을 상태 시스템에 연결했다.

**구현 및 판단**

- 투명화 중 제한 행동 중앙 검사
- 공격 이벤트 시 효과 중단
- 사망/Disconnect 등에서 남은 표현 정리

**결과**

NPC에는 스킬을 적용하지 않으며, 원격 Client에 필요한 최소 상태만 공개한다.

## 73. 발소리 공통 오디오 파이프라인

발걸음 소리를 스킬마다 별도 생성하지 않고 공통 CharacterAudioComponent에서 처리했다. 애니메이션 Notify와 캐릭터 상태를 연결했다.

**구현 및 판단**

- 보행/달리기 발소리 조건 구분
- SoundClass와 볼륨 정책 분리
- 부적절한 중복 AudioComponent 생성 방지

**결과**

발소리는 AnimNotify로 각 클라이언트에서 재생해, 발소리를 위한 RPC를 따로 보내지 않는다.

## 74. Footstep Amplify 다중 대상 처리

대상의 발소리를 증폭시키는 스킬을 추가했다. 복수 시전자 효과의 만료와 적용 순서를 분리하고 시각 경고에 필요한 정보만 전달했다.

**구현 및 판단**

- Caster/Target 효과 수명 관리
- 정해진 종료 시각에서 UI 남은 시간 계산
- 효과 조기 해제 알림 경로 재검토

**결과**

10단계 StatusHUD에 발소리 경고가 중복 출력되지 않게 반영했다.

## 75. Vision Distort PostProcess 연결

타겟에게 적용되는 시야 방해를 PostProcess 표현으로 처리했다. 종료 시각과 곡선 값은 Gameplay 판정과 UI에서 분리했다.

**구현 및 판단**

- Material Parameter 이름 `DM_LocalStartTime` 등 확정 항목 유지
- Vision 효과 Curve와 서버 시각 동기화
- 효과 종료 시 원상복귀

**결과**

효과 파라미터와 시간 보정은 대상 클라이언트의 로컬 표현에 두고, 서버는 적용과 종료만 승인한다.

## 76. Skill UI Read Model

Slot에 표시할 스킬 상태를 위젯이 직접 저장하지 않고 SkillComponent의 Read Model·Delegate를 통해 공급하도록 했다.

**구현 및 판단**

- 쿨다운·사용 가능 상태·선택 상태 읽기
- 사망/리스폰 재바인딩
- 10단계 SkillSlot에서 이름/설명/상태 텍스트 삭제

**결과**

UI가 Replication 원본을 새로 만들지 않는 경계를 유지했다.

## 77. 스킬과 사망·리스폰·Disconnect

Owner가 사망해도 SkillComponent 수명과 쿨다운을 유지하고, Pawn에 남은 비주얼 효과는 정리하도록 했다.

**구현 및 판단**

- Owner 사망·Target 사망 처리 구분
- PlayerState 소유 수명과 Character 표현 수명 분리
- Match Finished/Aborted에서 종료

**결과**

Pawn이 바뀔 때 시야 효과와 발소리 상태가 이전 Pawn에 남지 않도록 정리 경로를 묶었다.

## 78. Quest 보상 연결 작업 보류

8-11로 계획했던 Quest 완료 보상에 따른 Cooldown 복구를 구현 완료 처리하지 않았다.

**구현 및 판단**

- Quest 보상 정책 미확정으로 8-11 보류
- 이후 보상 자체를 제거하는 정책 결정
- 결과 기록에는 초기 제안과 최종 정책을 구분

**결과**

현재 Skill Cooldown은 Quest 보상으로 즉시 복원되지 않는다.

## 79. Editor 입력·스킬 에셋 연결 검수

Input Action, DataAsset, Material, Sound 등 C++ 코드 밖의 연결 항목을 별도로 정리했다.

**구현 및 판단**

- Skill DataAsset 3종 매핑
- Enhanced Input 바인딩
- PostProcess와 오디오 에셋 연결 점검


## 80. 스킬 멀티플레이·성능 최종 검수

8-15에서 타겟 선정, 비공개 정보, ServerTime 기반 Cooldown, Replication, 사망 후 정리 경로를 검수했다.

**구현 및 판단**

- Client 임의 타겟 전달 금지
- 매 프레임 Skill RPC/Replication 추가 금지
- 3종 효과의 종료 순서 확인

**결과**

효과 연출과 밸런스는 UI 통합 이후 플레이테스트에서 조정한다.
