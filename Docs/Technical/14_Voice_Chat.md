# 14. 음성 채팅 — Steam Voice와 생존 상태별 청취 정책

## 요구사항

음성은 이 게임에서 정체를 판별하는 단서다. 기획 규칙은 다음과 같다.

| 청취자 상태 | 규칙 |
|---|---|
| 로비 | 모두 같은 음량 |
| 생존 (`AliveProximity`) | 월드 거리 기반 감쇠. 범위 밖은 들리지 않는다 |
| 사망 (`DeadGlobal`) | 정상 접속한 모든 플레이어를 거리 감쇠·방향성 없이 같은 음량으로 듣는다 |
| 공통 | 말하는 사람 이름, 머리 위 음성 아이콘, 방향 UI를 표시하지 않는다 |

## 구조

`UDMVoiceChatSubsystem`(`UGameInstanceSubsystem`)이 맵 전환을 넘어 살아 있는 로컬 음성 관리자다. Steam `IOnlineVoice`로 송신을 켜고 끄며, 원격 플레이어마다 `UVOIPTalker`를 둬서 수신 정책을 적용한다. EOS VoiceChat API는 쓰지 않으므로 `VoiceChat`·`EOSVoiceChat` 플러그인은 비활성화했다.

```text
[ADMMatchPlayerController] 로컬 LifeState → EDMVoiceMode (AliveProximity / DeadGlobal)
      │ Delegate
      ▼
[UDMVoiceChatSubsystem]
  ├ 송신 정책: UpdateNetworkVoiceTransmission()   PTT / 음성 감지 / 음소거
  └ 수신 정책: RefreshRemoteTalkerPolicy()        0.25초 주기 + 모드 변경 시 즉시
        원격 PlayerState마다 UVOIPTalker 1개
          ├ AliveProximity: 원격 Pawn Root에 부착 + 근접 감쇠
          │                 위치를 모르면(Pawn 없음 / Relevancy 밖) → 로컬 음소거
          └ DeadGlobal·Lobby: 부착 없음, 감쇠 없음, 음소거 해제
```

송신은 `TogglePushToTalk`, `ToggleMicrophoneMuted`, `SetVoiceActivationEnabled`가 정책을 바꾸면 `UpdateNetworkVoiceTransmission`이 `StartNetworkedVoice` / `StopNetworkedVoice`를 호출한다. 엔진이 접속 초기화 때 송신을 강제로 끄는 경로와 충돌하던 문제는 [PTT 충돌](../Debugging/Network/11_SteamVoice_PTT_Conflict.md)에서 해결했다.

## 설계 결정

| 결정 | 이유 |
|---|---|
| 엔진 `UVOIPTalker` 사용 | Steam 음성이 원격 화자의 재생 컴포넌트를 만들 때 Talker 설정(부착 대상, 감쇠)을 적용한다. 오디오 경로를 직접 만들지 않는다 |
| **위치를 모르는 원격 플레이어는 음소거** | Talker에 부착 대상이 없으면 위치 없이 재생된다. 그러면 Relevancy 밖의 먼 플레이어가 **전역으로 들려 근접 규칙이 무너진다** |
| Talker 강한 참조를 직접 보관 | `CreateTalkerForPlayer()`가 만든 객체는 다른 곳에서 참조하지 않는다. GC에 회수되지 않도록 `UPROPERTY(Transient)` 배열에 보관한다 |
| 음소거 해제용 ID 별도 저장 | PlayerState가 먼저 파괴돼도 `FUniqueNetIdRepl`로 음소거를 풀어, 다음 월드에 음소거가 남지 않게 한다 |
| 시스템 전체가 아닌 로컬 음소거 | 플랫폼 차단 목록을 건드리지 않고 이 세션의 청취에만 적용한다 |
| 감쇠는 런타임 생성 + Config 값 | 300cm 안 최대 음량, 2,500cm 밖 무음. 맵에 맞춰 빌드 없이 ini에서 조정할 수 있게 했다 |
| 로컬 PlayerState 복제 전에는 갱신하지 않음 | 자신을 원격 화자로 착각해 Talker를 만들거나 음소거하는 것을 막는다 |

**수명 관리:** 경기 맵의 Controller에 바인딩될 때 주기 갱신을 시작한다. 로비 진입, 맵 이탈, 서브시스템 종료 때는 타이머 중지 → 음소거 전부 해제 → Talker 참조 해제 순으로 정리한다.

**알려 둘 동작:** Talker 설정은 원격 화자의 **다음 발화**가 시작될 때 재생 컴포넌트에 적용된다. 생존·사망 전환은 다음 발화부터 반영되고, 음소거는 즉시 적용된다.

## 관련 코드

- [DMMatchTypes.h](../../Source/DontMelt/Public/Core/DMMatchTypes.h) — `EDMVoiceMode`
- [DMVoiceChatSubsystem.cpp](../../Source/DontMelt/Private/Voice/DMVoiceChatSubsystem.cpp) — `InitializeLocalVoiceBackend`, `UpdateNetworkVoiceTransmission`, `EnterLobbyVoice`, `RefreshRemoteTalkerPolicy`, `GetOrCreateProximityAttenuation`
- [DMMatchPlayerController.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp) — `ClientEnableNetworkVoice_Implementation`, 관전 진입 시 `DeadGlobal`

## 핵심 코드

```cpp
// RefreshRemoteTalkerPolicy() 중 원격 플레이어별 처리
if (bUseProximity)
{
    const APawn* RemotePawn = RemotePlayerState->GetPawn();
    USceneComponent* AttachComponent = IsValid(RemotePawn) ? RemotePawn->GetRootComponent() : nullptr;

    // 원격 플레이어 Pawn 위치를 기준으로 거리 감쇠를 적용한다.
    // (설정은 다음 발화 시작 시점부터 오디오 컴포넌트에 반영된다.)
    Talker->Settings.ComponentToAttachTo = AttachComponent;
    Talker->Settings.AttenuationSettings = GetOrCreateProximityAttenuation();

    // 위치를 알 수 없는 원격 플레이어(Relevancy 밖 등)는
    // 근접 규칙상 들리지 않아야 하므로 로컬 음소거한다.
    SetRemoteTalkerLocallyMuted(Entry, AttachComponent == nullptr);
}
else
{
    // DeadGlobal·Lobby: 감쇠·방향성 없이 동일 음량으로 듣는다.
    Talker->Settings.ComponentToAttachTo = nullptr;
    Talker->Settings.AttenuationSettings = nullptr;

    SetRemoteTalkerLocallyMuted(Entry, false);
}
```

"범위 밖은 들리지 않는다"는 기획 한 줄이 **"위치를 모르는 대상은 어떻게 처리하는가"** 라는 구현 질문으로 이어졌다. 엔진의 기본 동작(위치 없이 재생)이 그대로 정보 누출이 되지 않는지 따져 본 결과가 위의 음소거 분기다.

---

[Technical 목차](README.md)
