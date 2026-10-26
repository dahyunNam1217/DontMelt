#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DMMatchTypes.h"

#include "DMPlayerResultRowWidget.generated.h"

class UTextBlock;

/**
 * 서버가 확정한 참가자 한 명의 결과를 표시한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMPlayerResultRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetResultEntry(const FDMMatchResultEntry& Entry);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_PlayerName = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Rank = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RelicCount = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_KillCount = nullptr;
};