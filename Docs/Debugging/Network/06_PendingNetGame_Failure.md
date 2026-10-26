# 접속 준비 단계(PendingNetGame)의 네트워크 실패를 놓치던 문제

| 항목 | 내용 |
|---|---|
| 문제 | IP 접속 실패나 맵 이동 중 연결 끊김에서 실패 안내 없이 화면만 멈췄다 |
| 원인 | 실패 이벤트의 `World`가 없으면 처리 함수가 첫 줄에서 바로 반환했다 |
| 해결 | 연결된 월드의 실패와 접속 준비 단계의 실패를 분리하고, Pending NetDriver의 소유자를 확인해 처리했다 |
| 관련 코드 | `UDMGameInstance::HandleNetworkFailure`, `IsOwnPendingNetDriver`, `HandlePendingConnectionFailure` |

## 현상

연결된 경기에서 서버가 종료되면 실패 안내와 메인 메뉴 복귀가 정상 동작했다. 반면 메뉴에서 접속을 시도하다 실패하거나, 맵 이동 도중 연결이 끊기면 안내가 나오지 않는 경로가 있었다.

## 원인 추적

접속이 완료되기 전에는 `UPendingNetGame`이 연결을 관리하므로 실패 이벤트의 `World`가 없다. 그런데 연결된 월드를 전제로 만든 검사식이 Pending 단계에도 그대로 적용되고 있었다.

```text
World 있음 → 내 월드인지 / NM_Client인지 / NetDriver 일치 여부 검사
World 없음 → 기존 코드에서는 곧바로 return
```

해결 방법을 찾는 과정에서 두 가지를 버렸다.

1. **엔진 함수 오버라이드:** `UGameInstance::HandleNetworkError()`를 오버라이드하려 했지만 `C3668`이 났다. UE5.5에서 이 함수는 virtual이 아니어서 확장 지점이 될 수 없었다.
2. **World가 없으면 무조건 처리:** 여러 PIE 인스턴스가 엔진의 실패 이벤트(`GEngine->OnNetworkFailure`)를 공유한다. `World == nullptr`이라는 이유만으로 처리하면 **다른 클라이언트의 실패까지 내 실패로 오인**한다.

## 해결

`HandleNetworkFailure()`를 두 경로로 나눴다.

| 경로 | 조건 | 처리 |
|---|---|---|
| 연결된 월드 | 이 GameInstance의 World이고 실제 NetDriver가 일치 | `HandleConnectedNetworkFailure()` |
| 접속 준비 단계 | World 없음 + `IsOwnPendingNetDriver()`가 true | `HandlePendingConnectionFailure()` |

`IsOwnPendingNetDriver()`는 엔진에 남은 PendingNetGame 컨텍스트를 먼저 조회하고, 이미 정리된 경우에는 접속을 시작할 때 기록해 둔 NetDriver 주소와 비교한다. 주소는 **비교 전용이며 역참조하지 않는다.** Pending 단계가 끝나면 기록을 지운다.

같은 작업에서 복구 뒤 입력 문제도 정리했다. 안내가 끝난 뒤 메뉴의 키보드·패드 포커스가 돌아오지 않는 경로가 있어, `FinishLocalFailureRecovery()`에서 입력 무시 상태를 풀고 `RestoreMainMenuAfterNotice()`에서 메뉴 입력 모드를 다시 적용하도록 했다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 없는 IP로 접속 | 실패 안내 후 메인 메뉴, 키보드 포커스 복구 |
| 맵 이동 도중 서버 종료 | 같은 안내 경로로 복귀 |
| PIE 2인스턴스 중 한쪽만 실패 | 실패한 인스턴스만 복구 화면으로 이동 |

## 정리

- 엔진 이벤트가 **어느 단계에서, 어떤 인자로** 오는지부터 확인해야 한다. 같은 이벤트라도 연결 전과 후의 인자가 다르다.
- 전역 이벤트를 받을 때는 "이 이벤트가 정말 내 것인가"를 판별하는 조건이 필요하다.
- 확장 지점을 찾을 때는 엔진 함수가 실제로 virtual인지 먼저 확인한다. 이 경험은 [빌드 오류 모음](../Build/02_Compile_Link_Errors.md)에도 남겼다.

---

[Debugging 목차](../README.md)
