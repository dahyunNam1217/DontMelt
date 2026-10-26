# Don't Melt — 시스템 아키텍처 설계

| 항목 | 내용 |
|---|---|
| 엔진 | Unreal Engine 5.5 |
| 언어 | C++ 중심, Blueprint는 에셋·UI·연출 연결 |
| 네트워크 | 2~4인 Listen Server / Steam OnlineSubsystem |
| 다루는 것 | 클래스 책임, 데이터 소유권, 네트워크 경계, 수명 계약 |

## 1. 설계 방향

이 게임은 상대의 정체와 진영을 숨기는 심리전이다. 서버가 판정할 데이터를 모든 클라이언트로 복제하면, HUD에서 감춰도 정보가 샐 수 있다. 그래서 **HUD에 표시하지 않는 것**과 **애초에 전송하지 않는 것**을 구분했다.

- GameMode가 규칙을 판단한다. GameState에는 공개 가능한 결과만 둔다.
- PlayerState에는 Pawn이 바뀌어도 유지할 정보만 둔다.
- Character는 이동·카메라·시각 표현과 Pawn 수명의 Component를 맡는다.
- 복제할 상태는 필요한 대상에게만 보낸다. 서버 내부 카운터와 로컬 애니메이션 값은 복제하지 않는다.
- 애니메이션처럼 매 프레임 계산이 필요한 곳은 Tick을 쓰고, 규칙의 탐색·카운트다운·AI 판단은 Timer와 Delegate로 구성한다.
- 실제 병목을 줄인다는 근거가 없으면 GAS, Mass, Replication Graph 같은 큰 구조를 먼저 넣지 않는다.

## 2. Gameplay Framework

| 클래스 | 책임 |
|---|---|
| `UDMGameInstance` | 월드 전환, 접속 실패·호스트 이탈 복구, 결과 캐시 |
| `ADMFrontendPlayerController` | 메인 메뉴 UI, 세션 요청, 남은 세션 정리 |
| `ADMLobbyGameMode` | 참가 승인, Ready·Host 검증, 시작 잠금, ServerTravel |
| `ADMLobbyGameState` / `ADMLobbyPlayerState` | 로비 공개 설정 / Ready·Host·표시 이름 |
| `ADMLobbyPlayerController` | 로컬 로비 UI, 서버 요청 |
| `ADMMatchGameMode` | 스냅샷, Phase, 사망 확정, 승패, 결과 이동 |
| `UDMRespawnCoordinator` | 30초 리스폰 수명주기 (GameMode 소유, 서버 전용) |
| `ADMMatchGameState` | 공개 Phase·종료 시각·공개 이벤트·결과 |
| `ADMMatchPlayerState` | LifeState·BaseSlot·스킬·퀘스트 등 지속 데이터 |
| `ADMMatchPlayerController` | 소유 클라이언트 입력, HUD, 개인 응답 |
| `UDMSpectatorPresentationComponent` | 관전 카메라 단계 (소유 클라이언트 전용) |

메뉴 → 로비 → 경기로 넘어가면 Controller와 PlayerState가 새로 만들어진다. 로비 Ready를 경기 PlayerState에 복사하거나 경기 LifeState를 로비로 가져오지 않는다.

## 3. Character와 Component

```text
ADMJellyCharacterBase
├─ ADMPlayerCharacter
│  ├─ UDMCombatComponent
│  ├─ UDMInteractionComponent
│  └─ UDMPoseComponent
└─ ADMNPCCharacter
   └─ ADMJellyAIController (서버 판단)

공용
├─ UDMCharacterMovementComponent
├─ UDMCharacterStateComponent
├─ UDMCharacterAudioComponent
├─ UDMJellyAnimInstance / ABP_DMJelly
└─ UDMJellyPhysicsComponent
```

`CharacterMovement`가 이동 상태와 예측·보정을 소유한다. 별도 이동 Enum을 복제하지 않는다. `UDMCharacterStateComponent`는 ActionState와 GameplayTag 상태 효과의 원본이다. 공격·방어 쿨다운은 Combat Component, 스킬 쿨다운은 PlayerState의 Skill Component가 맡는다. 관찰자용 스턴·투명화 bool은 표현용이고 판정 원본이 아니다.

`UDMJellyPhysicsComponent`는 사망 Ragdoll만 담당한다. 평소 젤리 흔들림은 `UDMJellyAnimInstance`의 감쇠 스프링과 `ABP_DMJelly`의 본 변형으로 처리한다. 물리 시뮬레이션이 캐릭터의 이동 권한을 대신하지 않는다.

## 4. 서버 시스템

| 시스템 | 원본 또는 권한 객체 | 제공하는 것 |
|---|---|---|
| 유물 상태 | `ADMRelic::WorldState` | 개별 유물 상태 |
| 운반·예약·등록 | `ADMRelicManager` | Pickup/Drop/Steal/Deposit Commit, 등록 수 조회 |
| 진영 영역 / 상자 | `ADMBaseZone` / `ADMRelicChest` | 진영 활성 여부 / 상호작용 지점 |
| 전역 퀘스트 / 개인 진행 | `UDMQuestDirector` / `UDMQuestProgressComponent` | 시작·마감·순위 / 개인 진행·실패 |
| 랜덤 이벤트 | `UDMRandomEventDirector` | 일정·중복 배제·종료 |
| NPC 인구 / 개별 행동 | `UDMNPCPopulationDirector` / `ADMJellyAIController` | 스폰·등록·집단 반응 / 타이머 기반 행동 |
| 스킬 | `UDMSkillComponent` | 사용 검증, 대상 선택, 쿨다운 |
| 승패 규칙 | `DMMatchResultRules` | 월드 없이 계산하는 판정·순위 |

```text
사용자 입력
  → 로컬 PlayerCharacter / PlayerController
  → Server RPC
  → 서버 Component/Manager가 요청 검증·Commit
  → MatchGameMode가 필요할 때 종료 판정
  → GameState / OwnerOnly 응답으로 허용된 정보 공개
  → UMG가 표시
```

GameState나 Widget에서 승리 판정을 다시 하지 않는다. RelicManager는 등록 성공을 알리지만, 최종 승리는 GameMode가 정한다.

## 5. 데이터 단일 원본

| 데이터 | 소유자 | 주의 |
|---|---|---|
| Match Phase·종료 시각 | GameState (서버가 작성) | 전환 권한은 GameMode |
| Match Result | 서버 확정 → GameState 복제 | Widget이 결과를 만들지 않음 |
| LifeState·BaseSlot | MatchPlayerState | Character에 중복 저장하지 않음 |
| ActionState·상태 효과 | CharacterStateComponent | 표현용 복제값과 구분 |
| Sprint·Jump·Velocity | CharacterMovement | 새 이동 Enum 불필요 |
| 방어 횟수·전투 쿨다운 | CombatComponent | 서버 검증 |
| 유물 상태 | Relic Actor | Manager의 운반자 인덱스와 일치 유지 |
| 등록 유물 수 | RelicManager가 유물 상태에서 계산 | 상자·GameState에 누적 캐시하지 않음 |
| 스킬 쿨다운 종료 시각 | PlayerState의 SkillComponent | 관전·리스폰 중 유지 |
| 로컬 화면·음량 설정 | Local Settings / GameUserSettings | 복제하지 않음 |
| 음성 송신·모드 | Voice Subsystem | 청취 정책과 구분 |

## 6. 복제와 정보 비공개

| 항목 | 전송 범위 |
|---|---|
| Phase·공개 퀘스트/이벤트·최종 결과 | 모든 클라이언트 |
| LifeState·BaseSlot·스킬 쿨다운·개인 퀘스트 진행 | 소유 클라이언트 |
| 상자 등록 수 | 상자를 연 플레이어에게만 서버 응답 스냅샷 |
| 상대 정체·유물 운반 관계·스킬 대상 | 복제하지 않음 |
| 유물 위치 공개·연장전 공개 | 정해진 시간 동안만 만든 데이터 |
| 스프링·본 회전·관전 카메라 좌표 | 로컬 계산, 복제하지 않음 |
| 관찰자용 스턴·투명화 표현 | 최소 bool |

남은 시간과 쿨다운은 종료 서버 시각을 복제하고 클라이언트가 차이를 계산한다. 작업 완료·결과 확정·서버 승인 요청은 저빈도 Reliable RPC나 OnRep으로 처리한다.

## 7. Steam 세션과 경기 수명

Steam 세션 생성, 로비 참가, SteamNetDriver 연결, 경기 서버 이동은 서로 다른 단계다. 검색 성공을 참가 성공으로, 참가 성공을 접속 성공으로 보지 않는다. 로비에서 Host와 Ready를 검증한 뒤 서버가 경기 맵으로 Travel하고, 경기가 끝나면 같은 참가자가 로비로 돌아온다.

`UDMOnlineSessionSubsystem`은 세션 생성·검색·참가·해제·갱신을, `UDMGameInstance`는 Network·Travel 실패 복구를 맡는다. 이 책임은 GameMode의 규칙 판정과 섞지 않는다.

## 8. 수명 계약

| 이벤트 | 반드시 정리할 상태 |
|---|---|
| 플레이어 사망 | 상호작용 세션·예약, Sprint 요청, 효과 표현, 운반 유물 |
| Pawn 교체 | Component Delegate, Controller의 이전 Pawn UI 참조 |
| 경기 종료 | 이벤트·퀘스트 타이머, 공격·스킬 입력, 공개 효과 데이터 |
| 경기 중단 | 중단 사유, 실패 안내, 세션·Travel 정리 |
| 로비 복귀 | 결과 캐시 확인, GameRoot 제거, 세션 광고 상태 |
| Network Failure | 의도한 이동과 구분, 중복 복구 차단, 메인 메뉴 복귀 |
| 음성 초기화·해제 | Talker, PTT 상태, 음소거, 청취 정책 |

OnRep은 바뀐 값을 전달하는 함수이지 서버 판정을 다시 실행하는 함수가 아니다. Pawn이 바뀌는 순간 Widget이 이전 Character를 계속 참조하지 않도록 Controller가 바인딩 수명을 관리한다.

## 9. 동시 사건의 처리 순서

유물 등록과 피격, 두 사람의 마지막 유물 등록, 방어와 공격의 경합은 모두 **서버에서 먼저 유효하게 Commit된 사건**을 기준으로 한다. 클라이언트 입력 순서나 UI 진행바 완료 시각으로 승자를 정하지 않는다. 유물 Commit 뒤 GameMode가 종료를 평가하고, `Finished`가 확정되면 이후 공격·등록·스킬 Commit을 받지 않는다.

같은 데이터를 두 시스템이 동시에 쓰지 않도록 원본을 고정했다. RelicManager는 등록 성공을 알리지만 결과를 쓰지 않는다. CharacterState는 기절 태그를 저장하지만 전멸 여부를 평가하지 않는다. 관전 카메라는 30초 뒤 화면을 되돌리지만 서버에 리스폰을 승인하지 않는다.

## 10. 설계 변경 기록

| 초기안 | 최종 |
|---|---|
| `EMovementState` 별도 저장 | CharacterMovement의 실제 값에서 해석 |
| 퀘스트 순위별 쿨다운 보상 | 보상 제거, 미완료 시 3D 사이렌 |
| 전신 액티브 래그돌 | 이동 중 절차적 젤리 보정 + 사망 시 Ragdoll |
| 상태 전량 복제 | OwnerOnly 원본과 관찰자용 최소 표현 분리 |
| 화면별 독립 위젯 생성 | GameRoot 레이어 + PlayerController 수명 관리 |
| NPC 배율 10 (최대 40) | 배율 15 (최대 60) |
| GameMode가 리스폰까지 처리 | `UDMRespawnCoordinator`로 분리 |
| PlayerController가 관전 연출까지 처리 | `UDMSpectatorPresentationComponent`로 분리 |

구현 세부는 [Technical](../Technical/README.md)에 있다.

관련 문서: [게임 기획서](GameDesign.md) · [입력 설계](EnhancedInput.md) · [UI 기획서](UIDesign.md)
