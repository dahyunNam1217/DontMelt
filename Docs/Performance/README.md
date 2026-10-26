# Performance — 성능 판단과 비용 관리

NPC가 최대 60명까지 늘어나는 Listen Server 게임에서, 측정으로 병목을 판단한 과정과 시스템별 비용 관리 방식을 정리했다.

| 문서 | 핵심 |
|---|---|
| [NPC가 프레임 저하의 원인인지 판단한 과정](01_NPC_Performance_Analysis.md) | 측정값으로 AI와 CharacterMovement 분리, 정적 검색으로 낭비 확인, 게임 규칙과 충돌하는 최적화를 쓰지 않은 이유 |
| [시스템별 비용 관리 설계](02_Cost_Design.md) | NPC 반응 상한, AI 타이머 스케줄링, OwnerOnly 복제, Sprint 예측 플래그, Tick 대신 타이머, UI 갱신 주기, Ragdoll 범위 |
| [프로파일링 계획과 최적화 후보 우선순위](03_Profiling_Plan.md) | 고정 조건, 시나리오 9개, 도구, 우선순위, 한 번에 하나씩 바꾸는 진행 원칙 |

[Docs 목차](../README.md)
