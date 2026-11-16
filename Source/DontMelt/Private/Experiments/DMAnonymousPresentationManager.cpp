#include "Experiments/DMAnonymousPresentationManager.h"

#include "Experiments/DMAnonymousPresentationActor.h"
#include "Characters/DMJellyCharacterBase.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMAnonymousPresentation, Log, All);

ADMAnonymousPresentationManager::ADMAnonymousPresentationManager()
{
    bReplicates = false;
    PrimaryActorTick.bCanEverTick = false;
}

bool ADMAnonymousPresentationManager::RegisterSource(APawn* Pawn)
{
    if (!HasAuthority() || !IsValid(Pawn) || !Pawn->HasAuthority() || Pawn->GetWorld() != GetWorld()
        || !IsValid(Cast<ADMJellyCharacterBase>(Pawn)))
    {
        return false;
    }
    const TWeakObjectPtr<APawn> Key(Pawn);
    if (const TWeakObjectPtr<ADMAnonymousPresentationActor>* Existing = Presentations.Find(Key))
    {
        if (Existing->IsValid())
        {
            return true;
        }
        UnregisterSource(Pawn);
    }
    if (Presentations.Num() >= 3)
    {
        UE_LOG(LogDMAnonymousPresentation, Error, TEXT("PoC supports at most three registered sources."));
        return false;
    }

    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // 소스 이름·Owner·Instigator·부착 관계를 표현 Actor로 복사하지 않는다.
    ADMAnonymousPresentationActor* Presentation = GetWorld()->SpawnActor<ADMAnonymousPresentationActor>(
        ADMAnonymousPresentationActor::StaticClass(), Pawn->GetActorTransform(), Parameters
    );
    if (!IsValid(Presentation))
    {
        UE_LOG(LogDMAnonymousPresentation, Error, TEXT("PoC presentation spawn failed."));
        return false;
    }

    Presentations.Add(Key, Presentation);
    Pawn->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleSourceDestroyed);
    UpdatePresentation(Pawn, Presentation);
    if (!GetWorldTimerManager().IsTimerActive(SampleTimer))
    {
        // 최대 세 소스의 연속 이동 스냅샷만 샘플링한다. 월드 검색이나 판정은 하지 않는다.
        GetWorldTimerManager().SetTimer(SampleTimer, this, &ThisClass::SampleSources, 0.05f, true);
    }
    return true;
}

void ADMAnonymousPresentationManager::UnregisterSource(APawn* Pawn)
{
    if (!HasAuthority() || Pawn == nullptr)
    {
        return;
    }
    TWeakObjectPtr<ADMAnonymousPresentationActor> Presentation;
    if (Presentations.RemoveAndCopyValue(TWeakObjectPtr<APawn>(Pawn), Presentation))
    {
        if (IsValid(Pawn))
        {
            Pawn->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSourceDestroyed);
        }
        if (Presentation.IsValid())
        {
            Presentation->Destroy();
        }
    }
    if (Presentations.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(SampleTimer);
    }
}

void ADMAnonymousPresentationManager::HandleSourceDestroyed(AActor* Source)
{
    UnregisterSource(Cast<APawn>(Source));
}

void ADMAnonymousPresentationManager::UpdatePresentation(APawn* Pawn, ADMAnonymousPresentationActor* Presentation)
{
    const ADMJellyCharacterBase* Character = Cast<ADMJellyCharacterBase>(Pawn);
    if (!IsValid(Character) || !IsValid(Presentation))
    {
        return;
    }
    FDMAnonymousPresentationState State;
    State.Location = Character->GetActorLocation();
    State.Rotation = Character->GetActorRotation();
    State.Velocity = Character->GetVelocity();
    if (const UDMCharacterStateComponent* Component = Character->GetCharacterStateComponent())
    {
        State.ActionState = Component->GetActionState();
    }
    if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        State.bFalling = Movement->IsFalling();
    }
    const AGameStateBase* GameState = GetWorld()->GetGameState();
    State.SampleServerTime = IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    Presentation->SetExcludedViewer(Cast<APlayerController>(Pawn->GetController()));
    Presentation->SetPublicState(State);
}

void ADMAnonymousPresentationManager::SampleSources()
{
    if (!HasAuthority())
    {
        return;
    }
    for (auto It = Presentations.CreateIterator(); It; ++It)
    {
        APawn* Pawn = It.Key().Get();
        ADMAnonymousPresentationActor* Presentation = It.Value().Get();
        if (!IsValid(Pawn) || !IsValid(Presentation))
        {
            if (IsValid(Pawn))
            {
                Pawn->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSourceDestroyed);
            }
            if (IsValid(Presentation))
            {
                Presentation->Destroy();
            }
            It.RemoveCurrent();
            continue;
        }
        UpdatePresentation(Pawn, Presentation);
    }
    if (Presentations.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(SampleTimer);
    }
}

void ADMAnonymousPresentationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(SampleTimer);
    for (const auto& Pair : Presentations)
    {
        if (APawn* Pawn = Pair.Key.Get())
        {
            Pawn->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSourceDestroyed);
        }
        if (ADMAnonymousPresentationActor* Presentation = Pair.Value.Get())
        {
            Presentation->Destroy();
        }
    }
    Presentations.Reset();
    Super::EndPlay(EndPlayReason);
}
