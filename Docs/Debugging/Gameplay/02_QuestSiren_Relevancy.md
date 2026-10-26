# 멀리 있던 플레이어가 퀘스트 실패 사이렌을 듣지 못하는 구조

| 항목 | 내용 |
|---|---|
| 문제 | 퀘스트 실패자의 위치를 알리는 사이렌을, 실패 시점에 멀리 있던 클라이언트는 듣지 못할 수 있었다 |
| 원인 | 시작을 `NetMulticast`로 알리고, 타이머로 one-shot 사운드를 반복 생성했다 |
| 해결 | 캐릭터에 `bQuestSirenActive` RepNotify 상태를 두고, 루프 AudioComponent 하나를 켜고 끄도록 바꿨다 |
| 관련 코드 | `ADMPlayerCharacter::SetQuestSirenActive`, `UDMQuestDirector` |

## 현상

퀘스트를 제한시간 안에 끝내지 못한 캐릭터는 주변에 3D 사이렌을 울려 위치를 드러낸다. 사이렌은 순간 효과가 아니라, 실패 상태가 유지되는 동안 **계속 켜져 있어야 하는 상태**다. 그런데 구현은 순간 효과처럼 되어 있었다.

## 원인 추적

| 구분 | 기존 처리 | 영향 |
|---|---|---|
| 시작 전달 | `MulticastStartQuestFailureWarning()` | 이후 Relevant해진 클라이언트에 시작 상태가 다시 전달되지 않음 |
| 오디오 생성 | 타이머마다 `SpawnSoundAttached()` | AudioComponent가 계속 생기고 타이머 관리가 복잡함 |
| 음량 | SoundClass 하위 클래스 | `bApplyToChildren=false`이면 SFX 슬라이더를 상속받지 못할 수 있음 |

[투명화 Relevancy 문제](01_Invisibility_Relevancy.md)와 같은 원인이다. 실패자의 세부 퀘스트 진행도까지 공개할 필요는 없었고, 필요한 정보는 "이 캐릭터가 사이렌 상태인가"뿐이었다.

## 해결

```cpp
void ADMPlayerCharacter::SetQuestSirenActive(const bool bActive)
{
    if (!HasAuthority() || bQuestSirenActive == bActive)
    {
        return;
    }

    bQuestSirenActive = bActive;
    ForceNetUpdate();
    RefreshQuestSirenPresentation(); // Listen Server Host의 표현도 직접 갱신
}
```

- 캐릭터마다 `QuestSirenAudioComponent`를 한 번만 만든다. 클라이언트는 복제된 상태로 루프 사운드의 `Play()`와 `Stop()`만 전환한다.
- `UDMQuestDirector`는 시작·종료 시 `SetQuestSirenActive()`만 호출한다.
- 실패 후 완료, 사망, 퇴장, 경기 종료가 모두 같은 함수로 사이렌을 끈다.
- Listen Server 호스트는 OnRep이 오지 않으므로 서버에서 값을 바꾼 직후 표현을 직접 갱신한다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| Relevancy 밖에서 실패 발생 → 접근 | 접근 후 사이렌이 들림 |
| 실패자가 퀘스트 완료·사망·퇴장 | 사이렌 중단 |
| 두 캐릭터 동시 실패 | 각자 위치에서 따로 들림 |
| SFX·Master 음량 조절 | 사이렌 음량에 반영 |

## 정리

- 오래 유지되는 효과를 반복 RPC나 반복 생성으로 만들지 않는다. **상태 하나를 복제하고, 표현은 클라이언트가 상태로부터 만든다.**
- Listen Server 호스트는 OnRep을 받지 않는다는 점을 서버 Setter에서 함께 처리한다.

---

[Debugging 목차](../README.md)
