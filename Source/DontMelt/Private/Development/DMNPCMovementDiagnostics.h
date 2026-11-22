#pragma once

#if WITH_EDITOR
class UCharacterMovementComponent;
struct FHitResult;

/** 명시적으로 켠 PIE 관찰만 기록한다. 이동/충돌/AI 상태를 변경하지 않는다. */
namespace DMNPCMovementDiagnostics
{
    void RecordMovement(UCharacterMovementComponent* Movement, float DeltaSeconds);
    void RecordStuck(UCharacterMovementComponent* Movement, const FHitResult* Hit);
}
#endif
