# 15. 설정 저장과 접속 장애 복구 — 맵 전환을 넘는 로컬 상태

## 설정의 소유자

`SetLanguageCode`는 en/ko로 정규화한다. 문화권 적용에 성공하면 메모리의 SaveGame 객체를 갱신하고, 설정 적용 경로의 `SaveSettings`가 디스크에 저장한다. 해상도와 창 모드는 UE 그래픽 설정에서 관리하며, 확인이 필요한 변경은 미리 적용 후 확정하거나 되돌린다. 오디오 설정은 `DMGameInstance`의 월드 적용 경로를 거친다.

| 설정 | 소유자 | 저장 |
|---|---|---|
| 해상도·창 모드·그래픽 | `UGameUserSettings` | 엔진 설정 파일 |
| 볼륨(마스터·SFX·음악 0.0~1.0), 마우스 감도(0.1~3.0) | `UDMLocalSettingsSubsystem` | `UDMLocalSettingsSaveGame` |
| 음성 모드(PTT / 음성 감지), 마이크 음소거 | `UDMVoiceChatSubsystem` | 로컬 설정 |

로컬 설정은 PlayerState로 복제하지 않는다. 서버는 다른 사람의 화면이나 음량을 알 필요가 없다. 해상도·창 모드를 바꾸면 15초 확인창을 띄우고, 응답이 없으면 이전 설정으로 되돌린다. 탭 이동·닫기 때 저장하지 않은 변경값을 처리하는 흐름은 [설정창 문제](../Debugging/UI/01_Settings_Unsaved_Changes.md)에 정리했다.

## 장애 복구

일부 Steam 실패 안내는 현지화 가능한 `FText`를 사용한다. 이 변경은 접속 복구 상태 머신의 성공 결과와 별개다. P9은 Gather 설정·중간 산출물까지 확인한 진행 중 상태다. [기준 체크포인트의 구현·검증 상태](../../CodexContext/ImplementationStatus.md#p1-p9-체크포인트-구현과-검증)

`UDMGameInstance`는 `Init`에서 Network·Travel 실패 Delegate를 연결하고, 실패를 상황별로 구분한다.

| 상황 | 판별 | 처리 |
|---|---|---|
| 의도한 나가기 | `IntentionalDisconnectWorld`가 현재 World | 실패로 보지 않음 |
| 서버가 지시한 맵 이동 중 연결 종료 | `TravelURL` 또는 `PendingNetGame` 존재 | 무시 ([ServerTravel 오탐](../Debugging/Network/05_ServerTravel_ConnectionLost.md)) |
| 이미 접속한 경기에서 끊김 | World 있음, 자기 NetDriver | `HandleConnectedNetworkFailure` |
| 접속 준비 단계 실패 | World 없음, 자기 Pending NetDriver | `HandlePendingConnectionFailure` ([PendingNetGame](../Debugging/Network/06_PendingNetGame_Failure.md)) |
| Travel 실패 | `TravelFailure` 이벤트 | `HandleTravelFailure` |

`StartLocalFailureRecovery`는 중복 복구를 막는 플래그를 세우고, 사유를 안내하는 `UDMConnectionLostWidget`을 띄운 뒤 메인 메뉴로 돌아간다. 안내 위젯을 만들지 못해도 복귀 경로는 그대로 실행한다. 메인 메뉴로 돌아오면 남은 Steam 세션을 정리하고([잔여 세션](../Debugging/Network/02_Steam_Stale_Session.md)) 메뉴 입력 포커스를 다시 맞춘다.

방 퇴장(`RequestLeaveOnlineRoom`)은 세션 해제를 기다리되 8초 상한을 둔다. 세션 시스템 자체에도 작업별 30초 타임아웃이 있어, 화면 이동과 세션 상태가 따로 풀린다.

## 관련 코드

적용과 화면 모드 미리보기는 [DMSettingsWidget.cpp](../../Source/DontMelt/Private/UI/DMSettingsWidget.cpp)에 있다. 파이프라인의 입력 및 출력은 [Game_Gather.ini](../../Config/Localization/Game_Gather.ini)와 [Game_Compile.ini](../../Config/Localization/Game_Compile.ini)에서 확인할 수 있다.

- [DMLocalSettingsSubsystem.cpp](../../Source/DontMelt/Private/Core/DMLocalSettingsSubsystem.cpp) — `SetAudioVolumes`, `SetMouseSensitivity`, `SaveSettings`
- [DMGameInstance.cpp](../../Source/DontMelt/Private/Core/DMGameInstance.cpp) — `RequestLeaveOnlineRoom`, `HandleNetworkFailure`, `StartLocalFailureRecovery`, `HandleTravelFailure`, `HandlePostLoadMap`

## 핵심 코드

```cpp
// UDMGameInstance::HandleNetworkFailure - 연결된 월드의 실패 처리 앞부분
// 비컨·데모 등 다른 NetDriver의 오류는 방 연결 종료로 보지 않는다.
if (NetDriver != nullptr && NetDriver != World->GetNetDriver())
{
    return;
}

// 서버가 지시한 레벨 이동(ClientTravel) 중에 기존 연결이 닫히는 것은 정상 흐름이다.
// 새 연결이 실패하면 World가 없는 PendingNetGame 경로에서 따로 처리된다.
if (const FWorldContext* Context = GetWorldContext())
{
    if (!Context->TravelURL.IsEmpty() || Context->PendingNetGame != nullptr)
    {
        UE_LOG(LogDMGameInstance, Log, TEXT("Network failure ignored during client travel. Type=%s, Message=%s"),
            GetNetworkFailureName(FailureType), *ErrorString);
        return;
    }
}

HandleConnectedNetworkFailure(World, NetDriver, FailureType, ErrorString);
```

---

[Technical 목차](README.md)
