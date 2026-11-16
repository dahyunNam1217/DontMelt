#include "Experiments/DMAnonymousPoCGameMode.h"

#include "Experiments/DMAnonymousPresentationManager.h"
#include "Experiments/DMAnonymousPoCPlayerPawn.h"
#include "Experiments/DMAnonymousPoCNPCPawn.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMAnonymousPoC, Log, All);

ADMAnonymousPoCGameMode::ADMAnonymousPoCGameMode()
{
    DefaultPawnClass = ADMAnonymousPoCPlayerPawn::StaticClass();
    PlayerControllerClass = APlayerController::StaticClass();
    // PlayerState 복제를 숨기는 편법을 쓰지 않는다. Pawn 역참조 노출은 검증 대상이다.
    PlayerStateClass = APlayerState::StaticClass();
    GameStateClass = AGameStateBase::StaticClass();
    bUseSeamlessTravel = false;
}

bool ADMAnonymousPoCGameMode::EnsurePresentationManager()
{
    if (!HasAuthority() || !IsValid(GetWorld()))
    {
        return false;
    }
    if (!IsValid(PresentationManager))
    {
        PresentationManager = GetWorld()->SpawnActor<ADMAnonymousPresentationManager>();
    }
    return IsValid(PresentationManager);
}

void ADMAnonymousPoCGameMode::RegisterPlayer(APlayerController* Controller)
{
    ADMAnonymousPoCPlayerPawn* Pawn = IsValid(Controller) ? Controller->GetPawn<ADMAnonymousPoCPlayerPawn>() : nullptr;
    if (!EnsurePresentationManager() || !IsValid(Pawn) || !PresentationManager->RegisterSource(Pawn))
    {
        UE_LOG(LogDMAnonymousPoC, Error, TEXT("PoC player presentation registration failed; experiment is incomplete."));
    }
}

void ADMAnonymousPoCGameMode::StartPlay()
{
    Super::StartPlay();
    if (!EnsurePresentationManager())
    {
        UE_LOG(LogDMAnonymousPoC, Error, TEXT("PoC presentation manager unavailable."));
        return;
    }
    // 초기 로그인 순서 차이를 위해 한 번만 확인한다. 반복 월드 검색은 하지 않는다.
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        RegisterPlayer(It->Get());
    }
    if (!NPCPawn.IsValid())
    {
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ADMAnonymousPoCNPCPawn* NPC = GetWorld()->SpawnActor<ADMAnonymousPoCNPCPawn>(
            ADMAnonymousPoCNPCPawn::StaticClass(), NPCSpawnTransform, Parameters
        );
        if (!IsValid(NPC) || !PresentationManager->RegisterSource(NPC))
        {
            if (IsValid(NPC))
            {
                NPC->Destroy();
            }
            UE_LOG(LogDMAnonymousPoC, Error, TEXT("PoC NPC registration failed; spawned NPC rolled back."));
            return;
        }
        NPCPawn = NPC;
    }
}

void ADMAnonymousPoCGameMode::PreLogin(
    const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage
)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (ErrorMessage.IsEmpty() && GetNumPlayers() >= 2)
    {
        ErrorMessage = TEXT("This isolated PoC supports two players.");
    }
}

void ADMAnonymousPoCGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    RegisterPlayer(NewPlayer);
}

void ADMAnonymousPoCGameMode::Logout(AController* Exiting)
{
    if (IsValid(PresentationManager) && IsValid(Exiting))
    {
        PresentationManager->UnregisterSource(Exiting->GetPawn());
    }
    Super::Logout(Exiting);
}

void ADMAnonymousPoCGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(PresentationManager))
    {
        PresentationManager->Destroy();
        PresentationManager = nullptr;
    }
    if (NPCPawn.IsValid())
    {
        NPCPawn->Destroy();
        NPCPawn.Reset();
    }
    Super::EndPlay(EndPlayReason);
}
