# 12. Steam Voice·PTT·생존 상태별 청취

## 요구사항

음성은 NPC와 플레이어의 정체를 구분하는 게임플레이 단서다. 따라서 송신·청취 정책은 정확해야 하고, 말하는 사람을 표시하는 **닉네임/음성 아이콘/방향성 UI는 노출하지 않는다**.

| 상태 | 청취 정책 |
|---|---|
| 로비 | 정상 접속한 모두를 같은 음량으로 청취 |
| 생존 `AliveProximity` | 근접 음성. **300cm 이내 최대**, **2,500cm 이상 무음**; 중간 감쇠 |
| 사망 `DeadGlobal` | 연결된 모든 플레이어 음성 거리/방향 감쇠 없이 같은 음량으로 청취 |

## 소유자

`UDMVoiceChatSubsystem`(GameInstanceSubsystem)이 Steam `IOnlineVoice`, `UVOIPTalker`, 원격 플레이어별 재생·타이머·음소거 정책을 관리. `ADMMatchPlayerController`의 LifeState·경기/관전 전환 이벤트를 받아 모드 변경. PTT·음성감지·마이크 음소거는 로컬 설정이 보유.

## 송신·수신 동작

- V는 PTT(누르는 동안 송신), M은 마이크 음소거, 설정에서 음성 감지 모드 지원.
- `UpdateNetworkVoiceTransmission`이 `StartNetworkedVoice` / `StopNetworkedVoice`를 조정.
- 원격 `UVOIPTalker`는 사용자당 1개, 필요 시 위치가 있는 Remote Pawn Root에 부착 + 근접 감쇠.
- **Pawn이 없거나 Relevant하지 않아 위치를 모르는 원격 플레이어는 생존 청취자 쪽에서 로컬 음소거한다.** 위치 없이 재생되면 전역 음성이 되는 문제 방지.
- 로비/사망 모드에서는 부착·감쇠·로컬 음소거 해제.
- Talker에 설정한 감쇠는 발화 시작 시점에 적용될 수 있으므로 전환 중 기존 재생 오디오도 점검(문서 기록).
- 문서상 원격 정책 Refresh는 **0.25초** 주기 및 모드 변경 때 즉시.

## 수명 계약

- 각 Talker를 강한 참조(예: `UPROPERTY(Transient)`)로 보관해 GC로 사라지지 않게 함.
- 원격 PlayerState 파괴 후에도 음소거 해제가 가능하도록 `FUniqueNetIdRepl` 등 안전 식별자 저장.
- 로비 진입/월드 교체/세션 종료 때 타이머 정지→로컬 음소거 해제→Talker 해제.
- 네트워크 입장 초기화 코드의 PTT 상태 덮어쓰기를 검증.
- UE VoiceChat/EOSVoiceChat을 추가로 사용하지 않는 것이 기록된 정책이지만 현재 모듈·플러그인 설정은 실제 `.uproject`와 `Config` 확인.

## 완료 기준

2~4인 Steam 패키지에서 로비 대화, V PTT 시작·종료, M mute, 음성 감지, 300/2500cm 거리, 대상 Relevancy 상실, 사망 전역 청취·리스폰 근접 복귀, Talker 재생/해제, 호스트·클라이언트 오디오 모두 확인.

[음성 설계](../../Docs/Design/Stages/11_SteamSessionVoice.md) · [기술](../../Docs/Technical/14_Voice_Chat.md) · [PTT 이슈](../../Docs/Debugging/Network/11_SteamVoice_PTT_Conflict.md)
