# 쿨다운이 끝나자마자 누른 공격이 서버에서 거절되는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 원격 클라이언트가 쿨다운 끝을 노려 공격하면, 가끔 예측 모션이 0.1초 만에 끊겼다 |
| 원인 | 클라이언트는 **추정한** 서버 시간, 서버는 **실제** 시간으로 쿨다운을 판단해 경계에서 수십 ms 차이가 났다 |
| 해결 | 서버가 원격 클라이언트의 요청을 검사할 때만 0.1초 여유를 두었다 |
| 관련 코드 | `UDMCombatComponent::IsAttackCooldownReady`, `IsDefenseCooldownReady` |

## 현상

공격은 클라이언트가 **모션을 먼저 재생**(예측)하고 서버가 승인하는 구조다.

- 핑이 있는 원격 클라이언트에서만 발생했다. 호스트는 같은 기기라 오차가 없다.
- 방어는 예측 모션이 없어서 끊기지는 않지만, 입력이 무시됐다.
- 호스트 로그에 `Server attack request rejected. Reason=AttackCooldown`이 찍혔다.

## 원인 추적

```text
클라이언트: ServerTime 추정 = 로컬 시간 + 마지막으로 받은 차이값
  └ IsAttackCooldownReady() == true → 예측 모션 재생 → ServerRequestAttack
서버: 실제 시간 < AttackCooldownEndServerTime (수십 ms 차이)
  └ CanRequestAttack 실패 → ClientRejectPredictedAttack → 모션 중단
```

`GetServerWorldTimeSeconds()`는 클라이언트에서 **근사값**이고, 오차는 지연과 동기화 주기에 따라 달라진다. 양쪽이 같은 검사 함수를 공유하므로, 서버만 기준을 다르게 둘 방법이 없었다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. 클라이언트에서 늦게 판단 | 클라이언트 쪽에 여유를 빼 둠 | 입력이 굼떠진다 |
| B. 서버 승인 후 모션 재생 | 예측 제거 | 핑만큼 반응이 늦어져 조작감이 나빠진다 |
| **C. 서버 검사에만 작은 여유** | 원격 요청일 때 +0.1초 | **채택.** 실제 쿨다운 길이와 UI는 그대로이고, 오차 범위만 흡수한다 |

```cpp
namespace
{
    // 원격 클라이언트의 서버 시간 추정 오차를 흡수하는 쿨다운 여유(초).
    constexpr double RemoteCooldownToleranceSeconds = 0.1;

    // 서버가 원격 클라이언트의 요청을 검사할 때만 여유를 준다.
    // 호스트 본인과 클라이언트 로컬 검사는 기존과 같다.
    double GetRemoteCooldownTolerance(const UActorComponent* Component)
    {
        const APawn* OwnerPawn = Component != nullptr ? Cast<APawn>(Component->GetOwner()) : nullptr;

        const bool bServerCheckingRemoteClient =
            IsValid(OwnerPawn) && OwnerPawn->HasAuthority() && !OwnerPawn->IsLocallyControlled();

        return bServerCheckingRemoteClient ? RemoteCooldownToleranceSeconds : 0.0;
    }
}

bool UDMCombatComponent::IsAttackCooldownReady() const
{
    return DMGameplay::GetServerTimeSeconds(this) + GetRemoteCooldownTolerance(this)
        >= FMath::Max(AttackCooldownEndServerTime, PredictedAttackCooldownEndServerTime);
}
```

방어 쿨다운(`IsDefenseCooldownReady`)에도 같은 보정을 적용했다.

**조건 설계**

- `HasAuthority() && !IsLocallyControlled()`가 "서버가 남의 요청을 검사하는 중"을 정확히 가리킨다.
- 클라이언트 쪽 판단과 UI의 남은 시간 표시는 이 조건에 해당하지 않아 바뀌지 않는다.
- 호스트 본인은 원격 클라이언트가 아니므로 여유를 받지 않는다.

**트레이드오프:** 원격 클라이언트는 최대 0.1초 일찍 공격할 수 있다. 쿨다운이 수 초 단위라 경쟁에 의미 있는 차이가 아니라고 판단했다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| `NetEmulation.PktLag 100`에서 쿨다운 끝 연타 | 모션 끊김 없음 |
| 같은 조건의 호스트 로그 | `Reason=AttackCooldown` 거절이 거의 없음 |
| UI 쿨다운 표시 | 기존과 같음 |

## 정리

- 클라이언트 예측을 쓰면 **양쪽 시간 기준의 오차**를 설계에 넣어야 한다. 둘을 같은 시간이라고 가정하면 경계에서 틀린다.
- 보정은 **필요한 쪽(서버의 원격 검사)에만** 좁게 적용해 다른 경로에 영향이 없게 했다.

---

[Debugging 목차](../README.md)
