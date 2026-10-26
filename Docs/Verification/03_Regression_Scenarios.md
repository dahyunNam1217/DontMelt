# 회귀 테스트 시나리오와 테스트 도구

기능을 추가하거나 고칠 때마다 다시 확인하는 시나리오를 영역별로 모았다. 개별 수정의 검증 방법은 각 [Debugging](../Debugging/README.md) 문서에 있고, 이 문서는 **여러 기능에 걸쳐 반복 확인하는 항목**을 다룬다.

## 테스트 환경과 도구

| 목적 | 방법 |
|---|---|
| 게임플레이 | PIE Listen Server + Client 2~3개 |
| Steam 세션·음성 | Standalone 패키지 2대, 서로 다른 Steam 계정, 같은 패키지 |
| 네트워크 지연 | `NetEmulation.PktLag 100~500`, `NetEmulation.PktLoss` |
| Phase 동기화 | 각 창에서 `getall DMMatchGameState PhaseState`로 서버·클라이언트 값 비교 |
| 경기 흐름 단축 | 아래 테스트 명령 (Shipping에서는 본문이 비어 있음) |
| 승패 규칙 | Session Frontend → Automation → `DontMelt.Match.ResultRules` |

```text
로비   DMReady / DMUnready / DMStartMatch
경기   DMTestFinishMatch <BaseSlot> / DMTestAbortMatch
       DMTestStartOvertime / DMTestFinishOvertime / DMTestNPCPanic
```

테스트 명령은 긴 플레이 없이 특정 Phase로 들어가기 위한 재현 도구다. 명령 실행 후에는 서버 Phase 변경 → GameState 복제 → 클라이언트 UI → 타이머 정리 순서로 확인한다. 정상 플레이의 조건 검증을 대신하지는 않는다.

## 세션·연결

| 시나리오 | 확인할 결과 |
|---|---|
| Create → Find → Join → ConnectString → Travel | 단계별 성공 로그가 순서대로 찍힘 |
| 호스트 방 재생성, 같은 클라이언트 재입장 | 잔여 세션 없이 성공 |
| 경기 중 호스트 강제 종료 | 클라이언트에 연결 끊김 안내 후 메인 메뉴, 입력 포커스 복구 |
| 접속→호스트 종료→복귀를 반복 | 호스트 종료 1회당 복귀 콜백 1회 (Delegate 중복 바인딩 없음) |
| Join 실패, Travel 실패 | 각자 다른 안내 문구로 복귀 |
| 로비→경기→결과→로비 10회 | 이동 중 튕김 없음 |

## 매치 흐름

| 시나리오 | 확인할 결과 |
|---|---|
| Waiting → Countdown → Playing → Overtime → Finished | 모든 클라이언트의 `PhaseState`가 서버와 같음 |
| 정규 시간 단독 1위 / 공동 1위 / 전원 0개 | 즉시 승리 / 연장전 / 연장전 |
| 0초와 유물 등록이 같은 프레임 | 결과 하나만 확정 |
| 2·4인 동률, 연장전 종료 동률 | 승자 없음 처리 |
| 결과 확정 직후 Host·Client 종료 | 남은 쪽이 정상 복귀 |
| 재경기 (PIE와 패키지 각각) | 같은 맵으로 이동 (`UEDPIE_` 접두사 제거) |

## 전투·사망·리스폰

| 시나리오 | 확인할 결과 |
|---|---|
| 두 공격이 같은 대상에 동시에 적중 | 사망 처리·유물 Drop·리스폰 타이머가 한 번만 실행 |
| 정면·측면·후방, 장애물 뒤 대상 | Host와 Client 판정이 같음 |
| 방어 마지막 Block 직후 재공격 | 사망 |
| 등록 도중 사망 | 등록 취소, 유물 Drop |
| 관전 중 공격자가 사망하거나 퇴장 | KillerView가 DeathCam으로 대체 |
| 관전 종료 직전 경기 종료·연결 끊김 | 정리 함수가 중복 실행되지 않음 |
| 상자 열기·스킬 대상 선택 중 사망 후 리스폰 | 남은 IMC·모달 없음, 마우스 캡처 복구 |
| 사망 중 쿨다운이 끝남 | 리스폰 후 바로 사용 가능 (쿨다운은 PlayerState 수명) |

## 스킬·이벤트·퀘스트

| 시나리오 | 확인할 결과 |
|---|---|
| 스킬 효과 중 사망·리스폰 | 이전 Pawn의 효과가 남지 않음, 쿨다운 유지 |
| 스킬 종료 타이머 만료 직전 접속 종료 | 유효하지 않은 Pawn 접근 없음 |
| 대상 후보 0명 | Targeting 종료, 쿨다운 미소모 |
| 이벤트 도중 연장전·Host 종료 | 정전 조명·입력 반전이 남지 않음 |
| 퀘스트 진행 중 사망·접속 해제 | 다른 참가자의 진행에 영향 없음 |

## UI

| 시나리오 | 확인할 결과 |
|---|---|
| 참가자 입장·퇴장 | 로비 Row 수와 화면 일치 |
| 결과 확정 | 결과 Row 수 = 참가자 수 |
| 영상 설정 변경 후 15초 무응답 | 이전 해상도·창 모드로 복원 |
| 설정 변경 후 탭 이동·메뉴 닫기·맵 이동 | 확인창 경로 유지 |
| Pawn 교체 후 HUD | 이전 Pawn의 Delegate를 잡고 있지 않음 |

## 음성

| 시나리오 | 확인할 결과 |
|---|---|
| 두 계정 PTT 누름·뗌, 음소거 | 상대가 듣는 상태와 로컬 표시가 일치 |
| 생존자끼리 거리 변화 | 300cm 안 최대 음량, 2,500cm 밖 무음 |
| 사망 후 관전 | 거리와 관계없이 생존자 음성 청취 |
| 부활 | 근접 감쇠로 복귀 |
| 로비 복귀 | 로컬 음소거·Talker 정리, 로비 음성 정상 |

## 구조 점검 기준

새 상태나 기능을 추가할 때 함께 확인하는 기준이다. 코드 리뷰에서 반복해서 본 질문을 정리했다.

| 기준 | 질문 |
|---|---|
| 상태 원본은 하나 | 같은 사실을 GameMode·PlayerState·Character·Component가 각각 저장하지 않는가 (예: `bIsAlive`, `bIsStunned` 같은 중복 필드) |
| Phase 원본 | 클라이언트까지 공유하는 Phase는 GameState의 `PhaseState` 하나이고, GameMode는 전환 조건만 판단하는가 |
| Enum과 GameplayTag | 같은 사실을 두 표현이 각자 수정하지 않는가. 동시에 성립할 수 없는 값만 한 Enum에 묶었는가 |
| 상태 축 분리 | Movement · Action · LifeState · Status Effect처럼 동시에 성립할 수 있는 상태를 한 Enum에 넣지 않았는가 |
| 수명 | 리스폰 뒤에도 유지할 값은 PlayerState, 현재 Pawn의 값은 Character/Component에 있는가 |
| 서버 권한 | 클라이언트 요청은 소유권이 맞는 Actor의 Server RPC를 거치고, 서버가 조건을 다시 검사하는가 |
| Host 테스트 함정 | Listen Server 호스트에서만 확인하지 않았는가. 호스트는 OnRep을 받지 않는다 |
| 복제 범위 | 모두에게 / 소유자에게만 / 서버 전용 중 어디에 속하는지 정했는가 |
| 등록 검증 | `UPROPERTY(Replicated)`에 대응하는 `DOREPLIFETIME`과 `Super::GetLifetimeReplicatedProps` 호출이 있는가 |
| UI | Widget이 Gameplay 값을 직접 수정하지 않고 읽기만 하는가 |

---

[Verification 목차](README.md)
