# Steam·세션·음성

## 세션 생성·검색·참가는 서로 다른 성공이다

Online Session은 플레이어가 같은 방을 찾고 모이기 위한 정보를 다룰 뿐, 게임 월드 접속을 대신하지 않는다. Steam 참가 문제를 추적하면서 가장 먼저 정리한 것이 이 구분이었다.

| 단계 | 성공의 의미 |
|---|---|
| Find 성공 | 세션 목록을 받았다 |
| Join 성공 | 그 세션에 참가 처리가 됐다 |
| Travel 성공 | 실제로 서버 월드에 들어갔다 |

"방은 보이는데 들어가지지 않는다"는 이 세 단계 중 어디서 멈췄는지에 따라 원인이 전부 달랐다([참가 단계별 추적](../../Docs/Debugging/Network/01_Steam_Join_Stages.md)).

Don't Melt는 생성 때 `DM_PROJECT`, `DM_LOBBY_STATUS`, `DM_NETWORK_BUILD`를 세션 설정에 광고한다. 검색 때는 앞의 두 키를 Steam 서버 쪽 필터로 걸고, 받은 결과를 `IsCompatibleOpenLobby`에서 네트워크 빌드와 빈 슬롯까지 다시 검사한다. 공유 AppID 480에서는 다른 게임의 로비가 결과 상한을 채워 내 방이 밀려날 수 있어서 서버 쪽 필터가 필요했다([검색 필터](../../Docs/Debugging/Network/03_Steam_Lobby_Search_Filter.md)).

소스: [DMOnlineSessionSubsystem.cpp](../../Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp)

## 접속 주소 해석

Join이 끝나면 `GetResolvedConnectString`으로 서버 주소를 얻어 `ClientTravel`한다. 이 주소는 NamedSession에서 먼저 얻고, 실패하면 검색 결과 원본으로 다시 시도한다. 참가 직전에는 검색 결과의 Presence·Lobby 플래그를 생성 설정과 맞춘다.

소스: [DMOnlineSessionSubsystem.cpp](../../Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp) · [DMFrontendPlayerController.cpp](../../Source/DontMelt/Private/Framework/Frontend/DMFrontendPlayerController.cpp)

## 비동기 작업의 수명

세션 요청 함수의 반환값은 "작업을 시작했다"일 뿐이고, 결과는 완료 Delegate로 온다. `UDMOnlineSessionSubsystem`은 작업 하나를 다음 순서로 다룬다.

1. `CurrentOperation`이 비어 있는지 확인한다. 작업은 한 번에 하나만 진행한다.
2. 완료 Delegate를 등록하고 요청한다. 시작 자체가 실패하면 바로 정리한다.
3. 완료 콜백에서 작업 종류와 세션 이름이 맞는지 확인한 뒤 결과를 처리한다.
4. `ClearActiveDelegate()`로 핸들을 해제하고 작업 상태를 비운다.

Steam 콜백이 끝내 오지 않으면 3단계가 영원히 오지 않아 "다른 온라인 작업이 진행 중"이 풀리지 않았다. 그래서 GameInstance 타이머로 작업마다 30초 타임아웃을 걸고, 타임아웃 뒤에 늦게 온 콜백은 Delegate가 이미 해제돼 무시되게 했다([세션 타임아웃](../../Docs/Debugging/Network/04_Steam_Session_Timeout.md)).

소스: [DMOnlineSessionSubsystem.cpp](../../Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp)

## ServerTravel과 실패 복구

`ServerTravel`은 서버가 월드를 옮기고 접속한 클라이언트를 함께 데려간다. `ClientTravel`은 한 PlayerController를 지정한 주소로 보낸다. 로비 → 경기 이동은 `bUseSeamlessTravel = false`인 non-seamless travel이라, 클라이언트는 기존 연결을 끊고 새로 접속한다.

이 "정상적인 끊김"이 네트워크 실패 이벤트로 올라와, 일부 클라이언트가 "호스트 연결 끊김"으로 메인 메뉴에 튕긴 문제가 있었다. 엔진의 `TickWorldTravel` 흐름을 따라가 보니 `TravelURL`이 비워진 직후 `PendingNetGame`이 만들어지는 구간이 있었고, 두 값을 모두 확인해 이동 중 끊김을 거르게 했다([ServerTravel 오탐](../../Docs/Debugging/Network/05_ServerTravel_ConnectionLost.md)).

경기 → 로비 이동 전에는 결과가 클라이언트에 도착했는지 ACK로 확인한다. Travel 자체가 실패하면 `HandleTravelFailure`가 사유를 안내하고 메인 메뉴로 복구한다. 같은 이동을 반복 시도하지 않는다.

소스: [DMMatchGameMode.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) · [DMGameInstance.cpp](../../Source/DontMelt/Private/Core/DMGameInstance.cpp)

## Steam Voice와 Push-to-Talk

`IOnlineVoice`로 로컬 Talker를 등록하고 `StartNetworkedVoice()` / `StopNetworkedVoice()`로 송신을 켜고 끈다. PTT·음성 감지·음소거는 모두 `UpdateNetworkVoiceTransmission` 한 곳에서 송신 여부로 정리한다. 음성 감지 임계값은 `UVOIPStatics::SetMicThreshold()`로 적용한다.

엔진도 접속 초기화 때 `ClientEnableNetworkVoice`로 송신을 켜고 끈다. 음성 감지 모드인데 접속 직후 송신이 안 되던 문제는 이 엔진 경로가 게임의 송신 상태를 덮어썼기 때문이었고, `ClientEnableNetworkVoice_Implementation`을 오버라이드해 게임 정책을 다시 적용했다([PTT 충돌](../../Docs/Debugging/Network/11_SteamVoice_PTT_Conflict.md)).

소스: [DMVoiceChatSubsystem.cpp](../../Source/DontMelt/Private/Voice/DMVoiceChatSubsystem.cpp) · [DMMatchPlayerController.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp)

## 근접 음성과 청취 정책

거리에 따른 음량 감쇠와 "누구 목소리를 들을 수 있는가"는 다른 문제다.

| 모드 | 청취 |
|---|---|
| Lobby | 모두 같은 음량 |
| `AliveProximity` | 원격 Pawn에 `UVOIPTalker`를 부착하고 300cm~2,500cm 감쇠 |
| `DeadGlobal` | 감쇠 없이 모두 같은 음량 |

근접 모드에서 원격 Pawn이 없거나 Relevancy 밖이라 위치를 모르면 그 플레이어를 로컬 음소거한다. 부착 대상이 없는 Talker는 위치 없이 재생돼, 멀리 있는 사람 목소리가 전역으로 들리기 때문이다.

로컬 PlayerState가 복제되기 전에는 갱신하지 않는다. 자기 자신을 원격 화자로 착각해 Talker를 만드는 것을 막기 위해서다.

소스: [DMVoiceChatSubsystem.cpp](../../Source/DontMelt/Private/Voice/DMVoiceChatSubsystem.cpp)

## 볼륨 설정

마스터·효과음·BGM 볼륨은 `UDMLocalSettingsSubsystem`이 0~1로 제한해 저장하고, GameInstance의 SoundMix와 SoundClass로 적용한다. 마이크 감지 임계값처럼 음성 백엔드에 걸린 값은 음성 Subsystem이 적용한다. 저장하는 곳과 적용하는 곳을 나눠 두면, 설정창은 값만 바꾸고 각 시스템이 자기 방식으로 반영한다.

소스: [DMLocalSettingsSubsystem.cpp](../../Source/DontMelt/Private/Core/DMLocalSettingsSubsystem.cpp) · [DMVoiceChatSubsystem.cpp](../../Source/DontMelt/Private/Voice/DMVoiceChatSubsystem.cpp)

[DevLog 목차](../README.md)
