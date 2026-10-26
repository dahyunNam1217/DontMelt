# Refactoring — 구조 개선

동작은 유지하면서 책임·데이터 의미·실패 복구 구조를 바꾼 작업이다.

| 문서 | 핵심 |
|---|---|
| [GameMode·PlayerController 책임 분리](01_Match_Responsibility_Split.md) | 실행 위치와 수명을 기준으로 리스폰·관전·승패 규칙을 분리, 규칙은 순수 함수 + Automation Test |
| [로비 시작 잠금 실패 경로 정리](02_Lobby_StartLock.md) | 해제가 빠진 실패 분기 3곳, 호출 순서 버그, 재시도 상한 |
| [결과표 데이터 의미 분리](03_MatchResult_Data.md) | 한 필드에 섞인 두 의미, 서버 Commit 기준 집계, 공동 순위 |
| [코드 위생과 배포 전 정리](04_Code_Hygiene.md) | 매직 넘버, 틀린 로그, 인코딩·줄바꿈, Shipping 가드, PIE 경로 |

[Docs 목차](../README.md)
