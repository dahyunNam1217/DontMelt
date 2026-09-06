# 8일차 — Lobby Ready 및 Player 준비 상태 구현

## 오늘 한 일

플레이어가 Lobby에서 Match 시작 준비를 할 수 있도록 Ready 시스템을 구성했다. Ready 상태의 원본을 `ADMLobbyPlayerState`가 소유하도록 했다.

```text
Client
    ↓
Ready 요청
    ↓
Server
    ↓
LobbyPlayerState Ready 변경
    ↓
Replication
    ↓
다른 Client 확인
```

Host와 일반 Client가 서로 다른 값을 임의로 소유하지 않도록 구조를 정리했다.

## 복습 개념

- PlayerState Replication
- Client Request
- Server RPC
- Ready State
- Authority
- Single Source of Truth

## 오늘의 디버깅 / 검수

Client가 Ready 상태를 직접 확정하지 않고, Server가 최종 상태 변경을 수행하는지 확인했다.
