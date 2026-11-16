#include "Experiments/DMAnonymousPoCNPCPawn.h"

#include "Experiments/DMAnonymousPoCAIController.h"

ADMAnonymousPoCNPCPawn::ADMAnonymousPoCNPCPawn(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetReplicates(false);
    SetReplicateMovement(false);
    AIControllerClass = ADMAnonymousPoCAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

bool ADMAnonymousPoCNPCPawn::IsNetRelevantFor(
    const AActor* /*RealViewer*/, const AActor* /*ViewTarget*/, const FVector& /*SrcLocation*/
) const
{
    return false;
}

void ADMAnonymousPoCNPCPawn::Destroyed()
{
    ADMAnonymousPoCAIController* AI = Cast<ADMAnonymousPoCAIController>(GetController());
    Super::Destroyed();
    // Pawn 제거 뒤 서버 AI만 고아로 남지 않게 실험이 생성한 Controller도 정리한다.
    if (HasAuthority() && IsValid(AI))
    {
        AI->Destroy();
    }
}
