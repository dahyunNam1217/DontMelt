# Don't Melt — Codex 작업 지침

> 이 파일은 코드 수정 허가가 아니라 **작업 맥락·검증 절차**를 전달한다. 사용자가 현재 대화에서 명시한 지시가 우선한다.

## 시작 전 반드시 읽기

1. [`CodexContext/ProjectOverview.md`](CodexContext/ProjectOverview.md) — 게임 목적, 환경, 범위
2. [`CodexContext/Architecture.md`](CodexContext/Architecture.md) — 책임·원본 데이터
3. [`CodexContext/GameplayRules.md`](CodexContext/GameplayRules.md) — 게임 규칙
4. [`CodexContext/NetworkPolicy.md`](CodexContext/NetworkPolicy.md) — 권한·복제·정보 비공개
5. [`CodexContext/ImplementationStatus.md`](CodexContext/ImplementationStatus.md) — 확인된 증거와 미확인 항목
6. 작업 대상 [`CodexContext/FeatureSpecs/`](CodexContext/FeatureSpecs/README.md) 문서

## 자료 우선순위와 충돌 처리

- 사용자의 **현재 최신 명시 요구** > 현재 작업 범위의 실제 소스·설정·에셋에서 확인한 구현 사실 > 이 문서 묶음의 **목표 규칙** > 저장소의 기존 설계·기술 문서 > 오래된 개발 일지.
- **구현 사실**과 **목표 규칙**이 다를 때는 묵시적으로 한쪽을 정답으로 덮어쓰지 말 것. 차이를 파일·함수·값 단위로 설명하고, 요청된 수정 범위 안에서만 변경할 것.
- 이 문서 묶음의 최초 작성 근거는 `DontMelt_Portfolio(1).zip`의 기획·기술 문서다. 해당 ZIP에는 C++ 소스, `.uproject`, 바이너리 에셋이 없었다. **'문서에 구현 기록이 있다'는 사실을 '현재 소스 확인 완료'로 오인하지 말 것.**
- 특히 NPC 배율 10→15(최대 40→60), 플레이어 수별 유물 3/4/5, 이동 속도 관련 과거 기록에 변경점이 있다. `ImplementationStatus.md`의 상충 사항을 먼저 확인할 것.

## 코딩·검수 원칙

- Unreal Engine 5.5, C++ 중심. BP는 메시/머티리얼/이펙트/위젯/애니메이션 배치 및 표현에 사용.
- **Server Authority / Single Source of Truth / Least Replication / Event Driven** 유지.
- GameMode와 서버 전용 Manager가 게임 판정을 확정하며, 클라이언트는 입력 의도만 전달. HUD/AnimInstance는 판정 원본을 새로 만들지 말 것.
- 플레이어 정체·유물 운반 관계·진영별 등록 수 등 비공개 정보의 전역 복제 금지. `GameState`에 넣고 UI만 숨기는 것도 금지.
- 긴 주기 폴링과 무제한 Actor 검색, 불필요한 Tick/Reliable RPC/복제 상태를 추가하지 말 것. 추가가 필요하면 측정 근거와 정리 시점을 명시.
- 새 시스템을 만들기 전에 기존 Component, Manager, Delegate, DataAsset와 호출 경로를 탐색하여 중복을 피할 것.
- 소스 관련 답변에는 **정확한 파일 경로 + 클래스/함수(또는 region) + 삽입/수정/삭제 위치 + 수정 전·후 코드 + 검증 방법**을 제시.
- 클래스·함수·변수명 등 식별자를 제외한 **코드 주석은 한국어**로 작성.
- 사용자가 소스 수정 ZIP을 특별히 요구하지 않은 경우, 수정된 소스 ZIP을 만들지 말고 변경 방법과 코드를 대화에 제공할 것.
- 실제로 실행하지 않은 빌드·패키징·Steam 멀티 테스트를 '통과'라고 쓰지 말 것.

## 작업 시작 체크

1. 저장소에 `.uproject`, `Source/DontMelt`, `Config`, `Content`가 있는지 조사.
2. 작업 기능의 설계 문서와 실제 파일·심볼, Blueprint/DataAsset 기본값 비교.
3. 권한·복제·Actor 수명/맵 이동·Listen Host 특수 케이스 영향 분석.
4. 변경 범위를 명확히 한 뒤 코드 수정.
5. 코드 검색·컴파일·PIE 2~4인·Steam 패키지 검증 중 실제로 수행한 항목만 보고.

**불명확한 항목을 임의 구현하지 말고 `확인 불가` 또는 `설계 충돌`로 표시한다.**
