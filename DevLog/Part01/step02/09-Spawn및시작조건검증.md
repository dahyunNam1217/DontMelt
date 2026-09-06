# 9일차 — Player Spawn 완료 및 Match 시작 조건 검증

## 오늘 한 일

단순히 Client가 서버에 접속했다는 사실만으로 플레이어가 생존 중이라고 판단하지 않도록 게임 규칙을 명확하게 했다.

**확정 조건**: Player Character가 실제 Map에 Spawn 완료되어야 생존 플레이어로 처리한다.

다음 상태를 구분했다.

```text
Connection 완료
≠
Player Character Spawn 완료
≠
Match 참여 준비 완료
```

2~4명의 플레이어가 정상적으로 Spawn되고 Match 시작 조건을 만족했을 때만 게임을 시작하도록 Match Flow를 검수했다.

## 복습 개념

- Connection Lifecycle
- Player Spawn Lifecycle
- Possession
- PlayerState / Character 관계
- Match Start Condition
- Server Validation

## 오늘의 디버깅 포인트

접속은 되어 있지만 Character가 아직 생성되지 않은 상태를 정상 플레이어로 계산할 경우, Match 시작 인원이나 생존 인원 계산이 잘못될 수 있으므로 이를 분리했다.
