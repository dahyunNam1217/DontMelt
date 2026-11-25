# P3 개발 PIE와 NPC 이동 진단

NPC 이동을 조사할 때는 서버의 경로 요청과 클라이언트에서 관찰한 위치 변화를 구분한다. P3에는 두 인원의 경기 진입을 돕는 에디터 경로와 월드별 이동 기록을 남기는 진단 도구가 있다.

## 개발 PIE 진입 조건

| 항목 | 코드에서 허용하는 조건 |
|---|---|
| 빌드와 월드 | 에디터 전용 비 Shipping, PIE 월드 |
| 서버와 맵 | Listen Server, `/Game/_DontMelt/Maps/Test/MatchMap_T` |
| 에디터 옵션 | 한 프로세스, 플레이어 2인, Additional Server Game Options에 `DMDevPIE=1` 및 `ExpectedPlayers=2` |
| 진입 방식 | 커스텀 Play World URL 없이 일반 PIE 시작. Dedicated, Simulate, 관전 시작은 개발 진입 조건에서 제외 |

`DMDevPIEMatch::BuildStartParameters`는 서버용 PIE 시작 매개변수의 복사본에 URL 옵션을 보완한다. 소스 주석은 UE5.5.4 단일 프로세스 시작 경로에서 서버 옵션 전달을 보완하기 위한 변경으로 설명한다. 클라이언트의 일반 엔진 접속 경로는 유지한다. 테스트 소스에는 지원 조건과 별도 프로세스·3인·Dedicated 거부, 루프백 주소 판정이 있다. [PIE 시작 매개변수](../../Source/DontMelt/Private/Development/DMDevPIEMatch.cpp) · [자동화 테스트 정의](../../Source/DontMelt/Private/Tests/DMDevPIEMatchTests.cpp)

## 이동 기록 수집

1. 위 조건에 맞는 PIE를 준비하고 콘솔에서 `DM.NPCDiag.Start <실행태그>`로 진단을 켠다. 태그는 선택 사항이며 기본값은 `Run`이다.
2. 같은 이동 구간을 관찰한 뒤 `DM.NPCDiag.Stop`으로 기록을 마친다. 월드 종료 경로에서도 Timer와 경로 완료 Delegate를 해제한다.
3. `Saved/P3_11f/NPCDiag_<태그>_<시각>.tsv`를 열어 월드, NPC, 로컬 Role과 요청 ID를 기준으로 행을 연결한다.

수집기는 `MatchMap_T`의 PIE Listen Server와 Client 월드에 붙는다. 1초 간격의 Sample에는 시간·프레임·위치·속도·AI 상태·경로 상태·목적지가 들어간다. 경로 컴포넌트의 완료 Delegate는 요청 ID와 결과 코드를 남긴다. 서버에서 경로 컴포넌트를 확인하고 원격 관찰은 위치·속도와 연결해 해석한다. [수집기와 명령](../../Source/DontMelt/Private/Development/DMNPCMovementDiagnostics.cpp)

## 정체와 요청 실패 읽기

| 신호 | 해석 |
|---|---|
| `Intent` | 경로 상태가 Moving이고 목적지의 수평 거리가 100cm를 초과한다. 정지 또는 도착 부근은 후보에서 제외한다. |
| `NoProgress`와 `Candidate` | 새 요청이거나 기준점에서 수평으로 10cm 이상 이동하면 기준을 갱신한다. 같은 의도에서 3초 이상 기준이 유지되면 후보 플래그를 세운다. |
| `GoalOnNav` | 목적지 인근 NavMesh 투영 결과다. 이동 경로 전체의 유효성은 경로 관측과 함께 확인한다. |
| 요청 거부·`PathFinished`·충돌 | AI 상세 로그에서 요청 결과를 확인하고, 완료 이벤트를 같은 요청 ID에 연결한다. 충돌·끼임 로그는 정체 원인 후보를 좁히는 데 쓴다. |

`Candidate`는 조사할 구간을 표시한다. 장애물, 경로 요청 교체, 월드별 관측 차이 중 어느 것이 원인인지는 요청 완료·충돌·위치 로그를 함께 봐야 판단할 수 있다. [AI 이동 로그](../../Source/DontMelt/Private/Characters/DMJellyAIController.cpp) · [충돌과 끼임 로그](../../Source/DontMelt/Private/Characters/Components/DMCharacterMovementComponent.cpp)

## NavMesh A/B 비교 범위

`MatchMap_T_A_144`와 `MatchMap_T_B_176`은 별도로 보존한 실험 맵이다. 개발 PIE 자동 진입과 위 TSV 수집기는 모두 정확한 `MatchMap_T` 패키지 경로를 검사한다. 두 실험 맵의 A/B 비교나 4인 실측에는 별도의 지원 실행·수집 경로를 확인해야 한다. 맵 이름의 접미사로 실제 스폰 수를 판단하지 않는다. [A 맵](../../Content/_DontMelt/Maps/Test/Experiments/P3_11r/MatchMap_T_A_144.umap) · [B 맵](../../Content/_DontMelt/Maps/Test/Experiments/P3_11r/MatchMap_T_B_176.umap)

현재 체크포인트에는 진단 코드, 테스트 정의와 실험 맵이 있다. PIE 실행 결과와 A/B 이동 품질·성능 측정값은 미확인이다. 비교할 하드웨어·인원·동선·측정 구간은 기존 계획에서 관리한다. [측정 시나리오](../Performance/03_Profiling_Plan.md#p3-11r-실험-맵-비교) · [AI 구현](../Technical/08_NPC_AI.md)
