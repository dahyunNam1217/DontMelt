# Don't Melt 프로젝트 디버깅 기록

이 폴더는 오류 코드 중심 TIL이 아니라 **Don't Melt 프로젝트에서 수행한 런타임 검증, 구조 점검, 회귀 테스트, 성능 점검 기록**을 모은 문서다.

기록을 다음 세 종류로 구분한다.

```text
실제 검증
→ 실제 로그/명령/시나리오가 남아 있는 항목

회귀 테스트
→ 재발 방지를 위해 정의한 반복 검증 시나리오

구조/성능 점검
→ 실제 버그 발생을 주장하지 않고 잠재적 불일치·비용을 예방하기 위한 점검 기록
```

## 문서 목록

1. Replication 등록 누락 점검
2. Match Phase Client Replication 검증
3. Match Flow Console Command 검증
4. Listen Server 종료 후 Client 복귀 검증
5. Delegate 중복 등록 Regression Test
6. Server Authority 위반 점검
7. GameMode / GameState 상태 중복 점검
8. PlayerState / Character 데이터 중복 점검
9. Component 간 동일 상태 중복 저장 점검
10. Enum / Gameplay Tag 동일 상태 중복 점검
11. Movement State / Action State 충돌 점검
12. UI / Gameplay State 불일치 점검
13. 불필요한 Tick 사용 점검
14. 불필요한 반복문 / 반복 탐색 성능 점검
15. 불필요한 Replication 비용 점검
16. Logging Utility 과도한 추상화 여부 점검

## 작성 원칙

- 실제 발생하지 않은 오류를 발생 사례처럼 쓰지 않는다.
- Raw Log가 없으면 구체적인 수치나 결과를 만들지 않는다.
- 예방성 검토는 `점검 기록`으로 명시한다.
- 구조 문제는 `State Owner / Authority / Lifetime / Replication Scope`를 기준으로 좁힌다.
- 성능 문제는 측정값이 있을 때만 개선 효과를 주장한다.
