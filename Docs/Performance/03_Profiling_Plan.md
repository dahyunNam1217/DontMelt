# 프로파일링 계획과 최적화 후보 우선순위

[NPC 성능 분석](01_NPC_Performance_Analysis.md)과 [비용 관리 설계](02_Cost_Design.md)에서 정리한 후보를 실제 수치로 확인하기 위한 측정 조건, 시나리오, 진행 원칙이다.

## 고정할 측정 조건

| 항목 | 기록 값 |
|---|---|
| 코드 버전 | Git 커밋 SHA, 빌드 구성(Development / Shipping) |
| 실행 환경 | CPU·GPU·RAM·OS·드라이버 |
| 렌더링 | 해상도, 스크린 퍼센티지, VSync, Frame Limit, 그래픽 설정 |
| 네트워크 | PIE / 패키지, 호스트 PC, 연결 방식, RTT, 손실률 |
| 레벨·NPC | 맵 이름, 실제 스폰 수, DataAsset 배율, 레벨 스폰 지점 수 |
| 시간 | 워밍업 구간, 캡처 길이, 반복 횟수 |

NPC 수는 C++ 기본 배율 15 기준으로 2인 30명, 3인 45명, 4인 60명이다. 단일 PIE 프로세스는 서버와 클라이언트 비용이 섞이므로, 최종 수치는 패키지 실행에서 호스트와 클라이언트를 따로 캡처한다.

## 측정 시나리오

| ID | 테스트 | 주로 볼 구간 |
|---|---|---|
| PERF-01 | 4인 + NPC 60, 대기·기본 보행 | Game Thread, Animation, Skeletal Mesh |
| PERF-02 | 경기 시작·NPC 일괄 스폰 | 단일 프레임 스파이크, SpawnActor, 첫 경로 요청 |
| PERF-03 | NPC 60명 이동·패닉 이벤트 | PathFollowing, AIController, CharacterMovement |
| PERF-04 | 오인 공격으로 여러 NPC 반응 | `HandleWrongTargetAttack`, 후보 정렬, 경로 요청 |
| PERF-05 | 4인 동시 줍기·등록 | Server RPC, Interaction 검증 타이머 |
| PERF-06 | 스킬·퀘스트·랜덤 이벤트 동시 | GameState 복제, 로컬 UI, 조명 갱신 |
| PERF-07 | 다수 동시 사망·Ragdoll | Physics Thread, Body 수, Collision |
| PERF-08 | 4인 음성 대화 | Network, Audio |
| PERF-09 | 결과 화면 → 로비 복귀 3회 반복 | 레벨 전환, GC, Widget·Timer 잔존 |

## 도구

| 목적 | 도구 |
|---|---|
| 빠른 확인 | `stat unit`, `stat game`, `stat anim`, `stat net`, `stat audio`, `stat slate` |
| 프레임 타임라인 | Unreal Insights, `-trace=cpu,frame,bookmark` |
| 네트워크 | Networking Insights, `-trace=net -NetTrace=1` |
| 애니메이션·물리·UI | Animation Insights, Chaos 디버그, Slate Insights |

`stat fps` 하나로 판단하지 않는다. 프레임 시간과 병목 스레드(GT/RT/GPU)를 함께 본다. 평균 외에 P95와 최댓값을 기록해 스폰·이벤트 순간의 스파이크를 확인한다.

## 최적화 후보 우선순위

| 순위 | 대상 | 근거 | 실험 |
|---|---|---|---|
| 1 | NPC 애니메이션 | NPC 수만큼 `NativeUpdateAnimation()`과 포즈 평가 | NPC만 URO 적용 전후 비교 (발소리 타이밍 함께 확인) |
| 1 | NPC 이동 복제 | Pawn 기본 NetUpdateFrequency 100 | NPC만 30/10으로 낮춘 전후 Network Insights 비교 |
| 1 | 초기 NPC 스폰 | 한 초기화 단계에서 SpawnActor 일괄 실행 | 스파이크가 크면 프레임 분산 스폰 실험 |
| 2 | NPC 이동 | 서버의 CharacterMovement 바닥 판정 | NPC만 NavWalking |
| 2 | Ragdoll | 사망 후 물리 Body 유지 | Sleep·정리 시점 조정 |
| 2 | UI 타이머 | 0.05~0.25초 타이머가 화면별로 존재 | Slate Insights로 중복 갱신 병합 필요성 판단 |
| 3 | 군집 발소리 | 동시 재생 수 | 사운드 Concurrency |
| 3 | 블랙아웃 조명 | LightComponent 다수 갱신 | GPU·그림자 비용 확인 |
| 3 | 유물 복구 검사 | 1초 주기 위치·바닥 검사 | Trace 비용 확인 후 간격 조정 |

## 진행 원칙

1. 최적화 전에 기준선을 기록한다.
2. CVar·DataAsset·코드 옵션을 **한 번에 하나씩** 바꾸고, 같은 빌드·맵·시점·NPC 수로 다시 측정한다.
3. 개선율은 `(기존 FrameTime − 수정 후 FrameTime) / 기존 FrameTime`으로 계산하고, 조건이 다르면 비교하지 않는다.
4. 게임플레이 단서(발소리, NPC 이동의 자연스러움, 스턴 표현, 관전 카메라)가 유지되는지 같이 확인한다.
5. 트레이스, 로그, 커밋을 함께 남긴다.

| 바꿀 때 특히 볼 회귀 | 이유 |
|---|---|
| NPC Anim 갱신을 줄임 | 가까운 NPC의 자세·발소리까지 영향을 받으면 안 된다 |
| 복제 빈도를 낮춤 | 도주·패닉에서 NPC 위치가 끊겨 보일 수 있다 |
| Ragdoll을 일찍 정리함 | 관전 카메라의 시체 중심 구간과 충돌할 수 있다 |
| UI 타이머를 늦춤 | 진행바가 끊겨 보일 수 있다 |

---

[Performance 목차](README.md)
