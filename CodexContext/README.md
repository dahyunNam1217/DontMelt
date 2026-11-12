# Don't Melt — Codex 맥락 문서

이 폴더는 Don't Melt 개발 저장소에 함께 보관할 **코드 검수·수정용 컨텍스트**다. 기존 포트폴리오용 `Docs/`를 대체하지 않으며, 게임 목표·아키텍처·규칙·네트워크 계약을 짧고 분명한 단위로 묶었다.

| 순서 | 문서 | 역할 |
|---|---|---|
| 1 | [ProjectOverview.md](ProjectOverview.md) | 목적, 기술, 범위, 맵/에셋 요구 |
| 2 | [Architecture.md](Architecture.md) | 책임과 단일 데이터 원본, 수명 |
| 3 | [GameplayRules.md](GameplayRules.md) | 수치와 상태 전이 등 게임 계약 |
| 4 | [NetworkPolicy.md](NetworkPolicy.md) | 권한, RPC, 복제, 개인정보성 게임 정보 차단 |
| 5 | [ImplementationStatus.md](ImplementationStatus.md) | 소스 확인 여부, 구현 기록, 미완성·제외 범위 |
| 6 | [FeatureSpecs/](FeatureSpecs/README.md) | 기능별 상세 최종 목표 및 검증 기준 |

## 증거 범위

- 원본: 대화에 첨부된 `DontMelt_Portfolio(1).zip`의 `Docs/Design`, `Docs/Technical`, `Docs/Performance`, `Docs/Verification`, `Docs/Debugging`, `DevLog`.
- **분석 가능한 C++ 소스/엔진 에셋/실행 파일은 이 아카이브에 포함되지 않음.** 문서 안의 코드 조각과 파일 링크는 구현 기록이지 현재 HEAD에 존재한다는 보증이 아니다.
- 날짜: 문서 묶음 작성 2026-11-12. 원본 아카이브의 Git 커밋·소스 기준 리비전은 확인 불가.

## 작업 순서

(1) `AGENTS.md`와 이 폴더 문서 읽기 → (2) 현재 `Source/`, `Content/`, `Config/` 탐색 → (3) 설계 대비 실제 구현 차이 보고 → (4) 명시적 수정 요청 범위만 변경 → (5) 검증 로그와 근거 남기기.

**경고:** 이 문서들은 '확정된 설계 목표'를 정리한 것이지 현재 빌드가 모든 기능을 갖췄다는 인증서가 아니다.
