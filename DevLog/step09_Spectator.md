# 9단계 — 사망 후 관전

## 81. 사망 후 관전 흐름 확정

실제 Kill Replay 녹화를 구현하는 대신 시체를 중심으로 보는 DeathCam과 현재 시점 관전을 사용하기로 했다. 서버 Respawn과 Local Camera 연출을 분리했다.

**구현 및 판단**

- 0~5초 시체 중심
- 5~10초 Killer Live View
- 10~25초 제한 자유 관전
- 25~30초 암전·카운트다운

**결과**

영상 버퍼링이나 실제 과거 화면 재생은 이번 단계의 구현이 아니다.

## 82. LifeState와 SpectatorPhase 책임 분리

사망 여부는 PlayerState의 LifeState에서만 결정하고, 관전 화면 단계는 소유 Client 로컬 상태로 뒀다.

**구현 및 판단**

- `EDMSpectatorPhase`와 관전 Context 구분
- Client 관전 단계가 서버 사망 상태를 바꾸지 않음
- 관전 진입 시간은 서버 시각 기준

**결과**

로컬 카메라 전환을 Gameplay 판정 RPC와 혼합하지 않았다.

## 83. Corpse DeathCam과 벽 충돌 회피

첫 5초는 사망한 플레이어의 시체 중심으로 카메라를 유지하도록 했다. 근처 벽에 카메라가 끼는 경우도 고려했다.

**구현 및 판단**

- Focus 위치와 Camera Offset 계산
- Wall Clipping 방지
- Ragdoll은 실제 Death 상태에서만 작동

**결과**

시체가 쓰러지는 물리 품질 문제는 11.5 젤리 물리 작업에서 추가 검토했다.

## 84. Killer Live View와 대체 대상

사망 후 5~10초에는 공격자의 현재 Pawn을 바라보도록 했다. 킬러가 사라지거나 자신도 사망하면 Fallback을 사용한다.

**구현 및 판단**

- Kill Replay가 아니라 Live View
- Killer Pawn 유효성 검사
- 카메라 대상 전환 실패 시 화면 비정상 상태 방지

**결과**

대상의 생존 상태나 새 Pawn 생성으로 관전 흐름이 멈추지 않도록 구조를 분리했다.

## 85. Restricted FreeCamera

10~25초 구간의 WASD+Mouse 자유 카메라를 별도로 구성했다. 지나치게 넓은 지역을 훑으며 생존자에게 정보를 주지 못하도록 공간과 정보 제한을 적용했다.

**구현 및 판단**

- Local 관전자 전용 Pawn 또는 ViewTarget 사용
- 서버 Gameplay Pawn Possession 변경 최소화
- 정보 제한이 적용되는 HUD/대상 표시 경로 확인

**결과**

다른 Client에게 관전자의 카메라 위치를 매 프레임 복제하지 않는다.

## 86. 관전 입력 Context 복원

관전용 Mapping Context를 등록했다가 실제 Respawn 성공 이후 Gameplay Context로 돌리는 흐름을 정리했다.

**구현 및 판단**

- 관전에서는 공격·유물·스킬 입력 제외
- 카메라 회전과 상하 이동 처리
- Input Focus와 UI 모달 충돌 확인

**결과**

리스폰 전에 단순 시간 경과만으로 게임 입력을 복원하지 않게 했다.

## 87. Fade와 5→1 카운트다운

마지막 5초는 화면 암전과 숫자 카운트다운으로 처리했다. 카메라 연출 타이머가 직접 Respawn을 호출하지 않게 했다.

**구현 및 판단**

- 25~30초 Black Fade
- 표시 숫자는 5,4,3,2,1
- 실제 Respawn은 서버 GameMode 전용

**결과**

카운트다운 UI는 10단계 GameRoot와 연결하는 별도 작업으로 남겼다.

## 88. 서버 Respawn 성공 이후 Handoff

서버의 새 Pawn 생성·Possess가 확인된 후 관전 ViewTarget과 Input을 정리하도록 했다.

**구현 및 판단**

- PlayerState LifeState 원본 유지
- Respawn 성공 신호 후 Local Spectator Cleanup
- 실패·지연 상태에서 관전 연출 유지

**결과**

클라이언트가 30초 타이머만 보고 임의로 살아난 것으로 처리하지 않는다.

## 89. 관전 정보 제한과 Voice 정책 경계

Dead Player가 생존자 위치를 추가로 알거나 이후 음성 정책을 우회하지 않도록 관전 제한 Gate를 검토했다.

**구현 및 판단**

- 비공개 퀘스트·스킬·유물 정보 노출 제한
- Stage 11 Voice Alive/Dead 정책을 위한 인터페이스 경계
- 실제 음성 재생 구현은 11단계로 이관

**결과**

관전 기능 자체의 완료와 Steam Voice 음성 보안을 구분했다.

## 90. 9단계 최종 Source·네트워크 검수

9-15까지의 코드 배치, 초기화·해제, Spectator Context, 서버 판정 RPC 노출 여부를 점검했다.

**구현 및 판단**

- 관전 중 게임플레이 RPC 차단 경계 점검
- Fade·카운트다운 WBP 연결은 10단계로

**결과**

관전 구조를 확정했다. 이후 관전 시작 RPC 호출 누락과 Handoff 시점을 다시 고쳤다([Handoff 경합](../Docs/Debugging/Gameplay/08_Respawn_Handoff.md)).
