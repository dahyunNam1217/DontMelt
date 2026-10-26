# Docs — 개발 문서

| 폴더 | 내용 | 먼저 볼 문서 |
|---|---|---|
| [Design](Design/README.md) | 게임 규칙, 시스템 아키텍처, UI·입력 설계, 0~11.5단계 설계 | [시스템 아키텍처 설계](Design/SystemArchitecture.md) |
| [Technical](Technical/README.md) | 시스템별 C++ 구현 — 상태 원본, 전달 경로, 실패 복구 | [복제와 RPC 기준](Technical/04_Replication_RPC.md) |
| [Debugging](Debugging/README.md) | 네트워크·게임플레이·UI·빌드 문제의 추적과 해결 (27건) | [Steam 참가 실패 단계별 추적](Debugging/Network/01_Steam_Join_Stages.md) |
| [Refactoring](Refactoring/README.md) | 책임 분리, 실패 경로 정리, 데이터 의미 분리, 코드 위생 | [GameMode·PlayerController 책임 분리](Refactoring/01_Match_Responsibility_Split.md) |
| [Performance](Performance/README.md) | NPC 성능 판단, 시스템별 비용 관리, 프로파일링 계획 | [NPC 성능 분석](Performance/01_NPC_Performance_Analysis.md) |
| [Verification](Verification/README.md) | 전체 소스 정적 검수 방법, 패키징 체크리스트, 회귀 시나리오 | [정적 검수와 묶음 단위 적용](Verification/01_Static_Review_Process.md) |

모든 문제 해결 문서는 **문제 요약 → 현상 → 원인 추적 → 해결(검토한 안과 채택 이유) → 검증 방법 → 정리** 순서로 쓴다. 코드 링크는 저장소 루트의 `Source/DontMelt/`를 가리킨다.

[포트폴리오 첫 화면](../README.md) · [개발 일지](../DevLog/README.md)
