#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "DiscordRpcBlueprint.generated.h"

// unreal's header tool hates clang-format
// clang-format off

/**
* Ask to join callback data
*/
USTRUCT(BlueprintType)
struct FDiscordUserData {
    GENERATED_USTRUCT_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString userId;
    UPROPERTY(BlueprintReadOnly)
    FString username;
    UPROPERTY(BlueprintReadOnly)
    FString discriminator;
    UPROPERTY(BlueprintReadOnly)
    FString avatar;
};


DECLARE_LOG_CATEGORY_EXTERN(Discord, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscordConnected, const FDiscordUserData&, joinRequest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDiscordDisconnected, int, errorCode, const FString&, errorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDiscordErrored, int, errorCode, const FString&, errorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscordJoin, const FString&, joinSecret);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscordSpectate, const FString&, spectateSecret);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscordJoinRequest, const FDiscordUserData&, joinRequest);

// clang-format on

/**
 * Rich presence data
 */
USTRUCT(BlueprintType)
struct FDiscordRichPresence {
    GENERATED_USTRUCT_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString state;
    UPROPERTY(BlueprintReadWrite)
    FString details;
    // todo, timestamps are 64bit, does that even matter?
    UPROPERTY(BlueprintReadWrite)
    int startTimestamp;
    UPROPERTY(BlueprintReadWrite)
    int endTimestamp;
    UPROPERTY(BlueprintReadWrite)
    FString largeImageKey;
    UPROPERTY(BlueprintReadWrite)
    FString largeImageText;
    UPROPERTY(BlueprintReadWrite)
    FString smallImageKey;
    UPROPERTY(BlueprintReadWrite)
    FString smallImageText;
    UPROPERTY(BlueprintReadWrite)
    FString partyId;
    UPROPERTY(BlueprintReadWrite)
    int partySize;
    UPROPERTY(BlueprintReadWrite)
    int partyMax;
    UPROPERTY(BlueprintReadWrite)
    FString matchSecret;
    UPROPERTY(BlueprintReadWrite)
    FString joinSecret;
    UPROPERTY(BlueprintReadWrite)
    FString spectateSecret;
    UPROPERTY(BlueprintReadWrite)
    bool instance;
};

/**
 *
 */
UCLASS(BlueprintType, meta = (DisplayName = "Discord RPC"), Category = "Discord")
class DISCORDRPC_API UDiscordRpc : public UObject {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable,
              meta = (DisplayName = "Initialize connection", Keywords = "Discord rpc"),
              Category = "Discord")
    void Initialize(const FString& applicationId,
                    bool autoRegister,
                    const FString& optionalSteamId);

    UFUNCTION(BlueprintCallable,
              meta = (DisplayName = "Shut down connection", Keywords = "Discord rpc"),
              Category = "Discord")
    void Shutdown();

    UFUNCTION(BlueprintCallable,
              meta = (DisplayName = "Check for callbacks", Keywords = "Discord rpc"),
              Category = "Discord")
    void RunCallbacks();

    UFUNCTION(BlueprintCallable,
              meta = (DisplayName = "Send presence", Keywords = "Discord rpc"),
              Category = "Discord")
    void UpdatePresence();

    UFUNCTION(BlueprintCallable,
              meta = (DisplayName = "Clear presence", Keywords = "Discord rpc"),
              Category = "Discord")
    void ClearPresence();

    UFUNCTION(BlueprintCallable,
              meta = (DisplayName = "Respond to join request", Keywords = "Discord rpc"),
              Category = "Discord")
    void Respond(const FString& userId, int reply);

    UPROPERTY(BlueprintReadOnly,
              meta = (DisplayName = "Is Discord connected", Keywords = "Discord rpc"),
              Category = "Discord")
    bool IsConnected;

    UPROPERTY(BlueprintAssignable,
              meta = (DisplayName = "On connection", Keywords = "Discord rpc"),
              Category = "Discord")
    FDiscordConnected OnConnected;

    UPROPERTY(BlueprintAssignable,
              meta = (DisplayName = "On disconnection", Keywords = "Discord rpc"),
              Category = "Discord")
    FDiscordDisconnected OnDisconnected;

    UPROPERTY(BlueprintAssignable,
              meta = (DisplayName = "On error message", Keywords = "Discord rpc"),
              Category = "Discord")
    FDiscordErrored OnErrored;

    UPROPERTY(BlueprintAssignable,
              meta = (DisplayName = "When Discord user presses join", Keywords = "Discord rpc"),
              Category = "Discord")
    FDiscordJoin OnJoin;

    UPROPERTY(BlueprintAssignable,
              meta = (DisplayName = "When Discord user presses spectate", Keywords = "Discord rpc"),
              Category = "Discord")
    FDiscordSpectate OnSpectate;

    UPROPERTY(BlueprintAssignable,
              meta = (DisplayName = "When Discord another user sends a join request",
                      Keywords = "Discord rpc"),
              Category = "Discord")
    FDiscordJoinRequest OnJoinRequest;

    UPROPERTY(BlueprintReadWrite,
              meta = (DisplayName = "Rich presence info", Keywords = "Discord rpc"),
              Category = "Discord")
    FDiscordRichPresence RichPresence;
};
