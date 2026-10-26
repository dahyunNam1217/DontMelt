# 11 — Steam 세션·음성 통합

## 1. Steam 세션 책임

| 책임 | 담당 |
|---|---|
| Create / Find / Join / Destroy / Update | `UDMOnlineSessionSubsystem` |
| 호스트 준비·Ready·경기 시작 | Lobby Framework |
| 경기 이동과 종료 | 서버 GameMode, GameInstance |
| Network·Travel 실패 | GameInstance 복구 |

세션이 만들어졌다고 다른 계정에서 목록이 보이거나 참가된다는 뜻은 아니다. 검색 필터, Steam Presence, 로비 광고 상태, 접속 주소, 실제 SteamNetDriver 접속을 단계별로 따로 확인한다. 로비에서 경기로 이동할 때는 광고 상태와 참가 승인 범위를 바꾸고, 경기 후 기존 참가자의 로비 복귀를 받는다.

## 2. 단계별로 만난 문제

| 단계 | 문제 | 처리 |
|---|---|---|
| OSS 초기화 | PIE에서 Steam OSS가 올라오지 않음 | 온라인 테스트는 Standalone으로 분리 |
| 빌드 | `VoiceChat is not a C++ module` | Steam `IOnlineVoice` 경로만 쓰고 VoiceChat 플러그인 비활성화 |
| 검색 | 엔진 `BuildUniqueId`와 프로젝트 `DM_NETWORK_BUILD`를 같은 값처럼 비교 | 엔진 값은 OSS에 맡기고 프로젝트 키만 검사 |
| 참가 | Presence·UseLobbies 불일치로 `Result=5` | 참가 직전 플래그 정규화 |
| 접속 주소 | `Invalid session info` | NamedSession 실패 시 검색 원본으로 재해석 |
| 접속 | 상위 설정의 IP NetDriver가 먼저 선택될 수 있음 | `!NetDriverDefinitions=ClearArray` 후 재정의 |

자세한 추적 과정은 [Steam 참가 단계 분리](../../Debugging/Network/01_Steam_Join_Stages.md)에 있다. 이후 잔여 세션, 검색 필터, 작업 타임아웃, 이동 중 오탐 문제도 이어서 해결했다.

## 3. 음성 책임

음성 백엔드는 Steam `IOnlineVoice`다. 로컬 Talker 등록, 송신 전환, 음소거, 설정 저장, 원격 청취는 서로 다른 단계로 다룬다.

| 기능 | 구현 |
|---|---|
| 로컬 Steam 음성 초기화 | `InitializeLocalVoiceBackend` |
| PTT / 음성 감지 모드 | `UpdateNetworkVoiceTransmission`, 엔진 기본 PTT 처리 차단 |
| 마이크 음소거, 로컬 설정 저장·복원 | `UDMVoiceChatSubsystem` |
| 생존 근접 / 사망 전역 청취 | 원격 플레이어별 `UVOIPTalker` + 근접 감쇠 + 위치 불명 시 로컬 음소거 |

살아 있을 때는 월드 거리 감쇠(300cm 안 최대, 2,500cm 밖 무음), 사망 중에는 정상 접속한 모든 플레이어의 음성을 거리와 관계없이 듣는다. 사망자에게 신원을 드러내는 UI는 만들지 않는다.

## 4. 음성 입력과 설정

PTT는 `V`, 음소거는 `M`이다. 초기 입력 설계의 `M = PTT`에서 바꿨다. 음성 입력은 로비와 경기 모두에서 쓰도록 별도 Mapping Context에 둔다. PTT·음성 감지 선택과 설정 저장·복원, 연결 종료 때 Talker 정리를 같은 수명 경계로 묶었다. 음성 옵션은 로컬 설정이고 서버 규칙의 원본이 아니다.

## 5. 로비·이동·결과와의 접점

- 경기 시작 전에 Steam 로비 광고 상태와 참가 범위를 갱신한다. 실패하면 되돌리고 잠금을 푼다.
- 결과 화면의 Host·Client 버튼이 서버 Travel을 두 번 실행하지 않는다.
- 재경기 준비와 Steam 세션 수명은 분리한다.
- 호스트가 종료하면 클라이언트는 실패 안내 후 메인 메뉴로 돌아간다.

## 6. 이 단계에서 함께 처리한 것

| 항목 | 처리 |
|---|---|
| NPC 배율 10 → 15 | Director 허용 범위 1~60, 레벨에 4인 기준 60개 스폰 지점 필요 |
| 로비·결과 동적 Row | Row 클래스 지정과 GameState 바인딩 재시도 정리 |
| 경기 시작 후 마우스 캡처 | 일반 조작 진입 지점마다 입력 모드 복구 |
| 정전 조명 컴파일 오류 | 조명 컴포넌트 헤더와 타입 경계 정리 |
| 젤리 관성 애니메이션 | 11.5단계로 분리 |

관련 문서: [Steam 세션 구현](../../Technical/02_Steam_Session_Lobby.md) · [음성 구현](../../Technical/14_Voice_Chat.md)

[Stages 목차](README.md)
