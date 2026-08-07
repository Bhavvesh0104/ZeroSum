// Copyright 2022 Ellie Kelemen. All Rights Reserved.

#include "WeaponBase.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Math/UnrealMathUtility.h"
#include "FPSCharacterController.h"
#include "FPSCharacter.h"
#include "Camera/CameraComponent.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AWeaponBase::AWeaponBase()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetAutonomousProxy(true);
    bNetUseOwnerRelevancy = true;

    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    // Create first person mesh component
    MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetOnlyOwnerSee(true);
    MeshComp->SetOwnerNoSee(false);
    MeshComp->bCastDynamicShadow = false;
    MeshComp->CastShadow = false;
    RootComponent = MeshComp;

    // Create third person mesh component
    TPMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPMeshComp"));
    TPMeshComp->SetOwnerNoSee(true);
    TPMeshComp->bCastDynamicShadow = true;
    TPMeshComp->CastShadow = true;
    TPMeshComp->SetupAttachment(RootComponent);

    // Creating the skeletal meshes for our attachments ,making sure that they don't cast shadows and setting onlyownersee to true

    BarrelAttachment = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BarrelAttachment"));
    BarrelAttachment->CastShadow = false;
    BarrelAttachment->SetOnlyOwnerSee(true);
    BarrelAttachment->SetupAttachment(MeshComp);

    MagazineAttachment = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MagazineAttachment"));
    MagazineAttachment->CastShadow = false;
    MagazineAttachment->SetOnlyOwnerSee(true);
    MagazineAttachment->SetupAttachment(MeshComp, FName("Magazine"));

    SightsAttachment = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SightsAttachment"));
    SightsAttachment->CastShadow = false;
    SightsAttachment->SetOnlyOwnerSee(true);
    SightsAttachment->SetupAttachment(MeshComp);

    StockAttachment = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("StockAttachment"));
    StockAttachment->CastShadow = false;
    StockAttachment->SetOnlyOwnerSee(true);
    StockAttachment->SetupAttachment(MeshComp);

    GripAttachment = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GripAttachment"));
    GripAttachment->CastShadow = false;
    GripAttachment->SetOnlyOwnerSee(true);
    GripAttachment->SetupAttachment(MeshComp);
}

void AWeaponBase::PreInitializeComponents()
{
    Super::PreInitializeComponents();

    if (GetOwner() != nullptr)
    {
        SetOwner(GetOwner());
    }
}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        SetOwner(GetInstigator());
    }

    // Getting a reference to the relevant row in the WeaponData DataTable
    if (WeaponDataTable && (DataTableNameRef != ""))
    {
        WeaponData = *WeaponDataTable->FindRow<FStaticWeaponData>(FName(DataTableNameRef), FString(DataTableNameRef), true);
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("MISSING A WEAPON DATA TABLE NAME REFERENCE"));
    }

    // Self-initialize runtime data if it hasn't been injected yet, guaranteeing full ammo on spawn
    if (HasAuthority() && GeneralWeaponData.ClipCapacity == 0)
    {
        GeneralWeaponData.WeaponClassReference = GetClass();
        GeneralWeaponData.AmmoType = WeaponData.AmmoToUse;
        GeneralWeaponData.ClipCapacity = WeaponData.ClipCapacity;
        GeneralWeaponData.WeaponHealth = 100.0f;
        GeneralWeaponData.WeaponAttachments = WeaponData.DefaultAttachments;

        if (WeaponData.bHasAttachments && WeaponData.AttachmentsDataTable)
        {
            for (FName RowName : GeneralWeaponData.WeaponAttachments)
            {
                if (FAttachmentData* AttData = WeaponData.AttachmentsDataTable->FindRow<FAttachmentData>(RowName, RowName.ToString(), true))
                {
                    if (AttData->AttachmentType == EAttachmentType::Magazine)
                    {
                        GeneralWeaponData.AmmoType = AttData->AmmoToUse;
                        GeneralWeaponData.ClipCapacity = AttData->ClipCapacity;
                    }
                }
            }
        }
        
        GeneralWeaponData.ClipSize = GeneralWeaponData.ClipCapacity;
    }

    // Setting our default animation values
    // We set these here, but they can be overriden later by variables from applied attachments.

    if (WeaponData.FP_WeaponEquip)
    {
        FP_WeaponEquip = WeaponData.FP_WeaponEquip;
    }
    if (WeaponData.TP_WeaponEquip)
    {
        TP_WeaponEquip = WeaponData.TP_WeaponEquip;
    }
    if (WeaponData.BS_Walk)
    {
        WalkBlendSpace = WeaponData.BS_Walk;
    }
    if (WeaponData.BS_Ads_Walk)
    {
        ADSWalkBlendSpace = WeaponData.BS_Ads_Walk;
    }
    if (WeaponData.Anim_Idle)
    {
        Anim_Idle = WeaponData.Anim_Idle;
    }
    if (WeaponData.Anim_Sprint)
    {
        Anim_Sprint = WeaponData.Anim_Sprint;
    }
    if (WeaponData.Anim_Ads_Idle)
    {
        Anim_ADS_Idle = WeaponData.Anim_Ads_Idle;
    }

    // Setting our recoil & recovery curves
    if (VerticalRecoilCurve)
    {
        FOnTimelineFloat VerticalRecoilProgressFunction;
        VerticalRecoilProgressFunction.BindUFunction(this, FName("HandleVerticalRecoilProgress"));
        VerticalRecoilTimeline.AddInterpFloat(VerticalRecoilCurve, VerticalRecoilProgressFunction);
    }

    if (HorizontalRecoilCurve)
    {
        FOnTimelineFloat HorizontalRecoilProgressFunction;
        HorizontalRecoilProgressFunction.BindUFunction(this, FName("HandleHorizontalRecoilProgress"));
        HorizontalRecoilTimeline.AddInterpFloat(HorizontalRecoilCurve, HorizontalRecoilProgressFunction);
    }

    if (RecoveryCurve)
    {
        FOnTimelineFloat RecoveryProgressFunction;
        RecoveryProgressFunction.BindUFunction(this, FName("Client_HandleRecoveryProgress"));
        RecoilRecoveryTimeline.AddInterpFloat(RecoveryCurve, RecoveryProgressFunction);
    }

    // Attaching weapons to their respective character meshes
    if (AFPSCharacter *CurrentPlayer = Cast<AFPSCharacter>(GetOwner()))
    {
        MeshComp->AttachToComponent(CurrentPlayer->GetHandsMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetStaticWeaponData()->FP_WeaponAttachmentSocketName);
        SetTPAttachment();
    }
    
    // If OnRep fired too early, this ensures the magazine still spawns once WeaponData is locally available.
    SpawnAttachments();
}

void AWeaponBase::OnRep_Owner()
{
    Super::OnRep_Owner();

    // The Client now definitively knows who owns this weapon, so we can safely attach it
    if (AFPSCharacter* CurrentPlayer = Cast<AFPSCharacter>(GetOwner()))
    {
        MeshComp->AttachToComponent(CurrentPlayer->GetHandsMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetStaticWeaponData()->FP_WeaponAttachmentSocketName);
        SetTPAttachment();
        
        // Force immediate visibility check against the inventory to prevent secondary weapons from rendering on spawn
        if (UInventoryComponent* InvComp = CurrentPlayer->FindComponentByClass<UInventoryComponent>())
        {
            if (InvComp->GetCurrentWeapon() != this)
            {
                SetActorHiddenInGame(true);
                PrimaryActorTick.bCanEverTick = false;
            }
        }
    }
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(AWeaponBase, bOnlyOwnerSee, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AWeaponBase, bOwnerNoSee, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AWeaponBase, TPMeshComp, COND_SkipOwner);
    DOREPLIFETIME(AWeaponBase, GeneralWeaponData);
}

void AWeaponBase::OnRep_GeneralWeaponData()
{
    SpawnAttachments();
}

void AWeaponBase::SetTPAttachment()
{
    if (AFPSCharacter *CurrentPlayer = Cast<AFPSCharacter>(GetOwner()))
    {
        TPMeshComp->AttachToComponent(CurrentPlayer->GetThirdPersonMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetStaticWeaponData()->TP_WeaponAttachmentSocketName);
    }
}

void AWeaponBase::SpawnAttachments()
{
    //FIX: Verifying the DataTable pointer exists before dereferencing
    if (WeaponData.bHasAttachments && WeaponData.AttachmentsDataTable != nullptr)
    {
        for (FName RowName : GeneralWeaponData.WeaponAttachments)
        {
            // Going through each of our attachments and updating our static weapon data accordingly
            AttachmentData = WeaponData.AttachmentsDataTable->FindRow<FAttachmentData>(RowName, RowName.ToString(), true);

            if (AttachmentData)
            {
                DamageModifier += AttachmentData->BaseDamageImpact;
                WeaponPitchModifier += AttachmentData->WeaponPitchVariationImpact;
                WeaponYawModifier += AttachmentData->WeaponYawVariationImpact;
                HorizontalRecoilModifier += AttachmentData->HorizontalRecoilMultiplier;
                VerticalRecoilModifier += AttachmentData->VerticalRecoilMultiplier;

                if (AttachmentData->AttachmentType == EAttachmentType::Barrel)
                {

                    BarrelAttachment->SetSkeletalMesh(AttachmentData->AttachmentMesh);
                    WeaponData.MuzzleLocation = AttachmentData->MuzzleLocationOverride;
                    WeaponData.ParticleSpawnLocation = AttachmentData->ParticleSpawnLocationOverride;
                    WeaponData.bSilenced = AttachmentData->bSilenced;
                }
                else if (AttachmentData->AttachmentType == EAttachmentType::Magazine)
                {
                    MagazineAttachment->SetSkeletalMesh(AttachmentData->AttachmentMesh);
                    
                    if (AttachmentData->FiringSoundOverride) WeaponData.FireSound = AttachmentData->FiringSoundOverride;
                    if (AttachmentData->SilencedFiringSoundOverride) WeaponData.SilencedSound = AttachmentData->SilencedFiringSoundOverride;
                    
                    WeaponData.RateOfFire = AttachmentData->FireRate;
                    WeaponData.bAutomaticFire = AttachmentData->AutomaticFire;
                    
                    if (AttachmentData->VerticalRecoilCurve) WeaponData.VerticalRecoilCurve = AttachmentData->VerticalRecoilCurve;
                    if (AttachmentData->HorizontalRecoilCurve) WeaponData.HorizontalRecoilCurve = AttachmentData->HorizontalRecoilCurve;
                    if (AttachmentData->RecoilCameraShake) WeaponData.RecoilCameraShake = AttachmentData->RecoilCameraShake;
                    
                    WeaponData.bIsShotgun = AttachmentData->bIsShotgun;
                    WeaponData.ShotgunRange = AttachmentData->ShotgunRange;
                    WeaponData.ShotgunPellets = AttachmentData->ShotgunPellets;
                    
                    if (AttachmentData->EmptyWeaponReload) WeaponData.EmptyWeaponReload = AttachmentData->EmptyWeaponReload;
                    if (AttachmentData->WeaponReload) WeaponData.WeaponReload = AttachmentData->WeaponReload;
                    if (AttachmentData->WeaponIdle) WeaponData.WeaponIdle = AttachmentData->WeaponIdle;
                    if (AttachmentData->FP_EmptyPlayerReload) WeaponData.FP_EmptyPlayerReload = AttachmentData->FP_EmptyPlayerReload;
                    if (AttachmentData->TP_EmptyPlayerReload) WeaponData.TP_EmptyPlayerReload = AttachmentData->TP_EmptyPlayerReload;
                    if (AttachmentData->FP_PlayerReload) WeaponData.FP_PlayerReload = AttachmentData->FP_PlayerReload;
                    if (AttachmentData->TP_PlayerReload) WeaponData.TP_PlayerReload = AttachmentData->TP_PlayerReload;
                    if (AttachmentData->Gun_Shot) WeaponData.Gun_Shot = AttachmentData->Gun_Shot;
                    if (AttachmentData->ShotGun_Shot2) WeaponData.ShotGun_Shot2 = AttachmentData->ShotGun_Shot2;
                    if (AttachmentData->FP_Player_Shot) WeaponData.FP_Player_Shot = AttachmentData->FP_Player_Shot;
                    if (AttachmentData->TP_Player_Shot) WeaponData.TP_Player_Shot = AttachmentData->TP_Player_Shot;
                    if (AttachmentData->FP_Player_ADS_Shot) WeaponData.FP_Player_ADS_Shot = AttachmentData->FP_Player_ADS_Shot;
                    if (AttachmentData->TP_Player_ADS_Shot) WeaponData.TP_Player_ADS_Shot = AttachmentData->TP_Player_ADS_Shot;
                    
                    WeaponData.AccuracyDebuff = AttachmentData->AccuracyDebuff;
                    WeaponData.bWaitForAnim = AttachmentData->bWaitForAnim;
                    WeaponData.bPreventRapidManualFire = AttachmentData->bPreventRapidManualFire;
                }
                else if (AttachmentData->AttachmentType == EAttachmentType::Sights)
                {
                    SightsAttachment->SetSkeletalMesh(AttachmentData->AttachmentMesh);
                    VerticalCameraOffset = AttachmentData->VerticalCameraOffset;
                    WeaponData.bAimingFOV = AttachmentData->bAimingFOV;
                    WeaponData.AimingFOVChange = AttachmentData->AimingFOVChange;
                    WeaponData.ScopeMagnification = AttachmentData->ScopeMagnification;
                    WeaponData.UnmagnifiedLFoV = AttachmentData->UnmagnifiedLFoV;
                }
                else if (AttachmentData->AttachmentType == EAttachmentType::Stock)
                {
                    StockAttachment->SetSkeletalMesh(AttachmentData->AttachmentMesh);
                }
                else if (AttachmentData->AttachmentType == EAttachmentType::Grip)
                {
                    GripAttachment->SetSkeletalMesh(AttachmentData->AttachmentMesh);
                    if (AttachmentData->FP_WeaponEquip)
                    {
                        WeaponData.FP_WeaponEquip = AttachmentData->FP_WeaponEquip;
                    }
                    if (AttachmentData->TP_WeaponEquip)
                    {
                        WeaponData.TP_WeaponEquip = AttachmentData->TP_WeaponEquip;
                    }
                    if (AttachmentData->BS_Walk)
                    {
                        WalkBlendSpace = AttachmentData->BS_Walk;
                    }
                    if (AttachmentData->BS_Ads_Walk)
                    {
                        ADSWalkBlendSpace = AttachmentData->BS_Ads_Walk;
                    }
                    if (AttachmentData->Anim_Idle)
                    {
                        Anim_Idle = AttachmentData->Anim_Idle;
                    }
                    if (AttachmentData->Anim_Sprint)
                    {
                        Anim_Sprint = AttachmentData->Anim_Sprint;
                    }
                    if (AttachmentData->Anim_Ads_Idle)
                    {
                        Anim_ADS_Idle = AttachmentData->Anim_Ads_Idle;
                    }
                    if (AttachmentData->Anim_Jump_Start)
                    {
                        Anim_Jump_Start = AttachmentData->Anim_Jump_Start;
                    }
                    if (AttachmentData->Anim_Jump_End)
                    {
                        Anim_Jump_End = AttachmentData->Anim_Jump_End;
                    }
                    if (AttachmentData->Anim_Fall)
                    {
                        Anim_Fall = AttachmentData->Anim_Fall;
                    }
                }
            }
        }
    }
}

// Start Fire

void AWeaponBase::StartFire(FVector CameraLocation, FRotator CameraRotation)
{
    if (bCanFire)
    {
        // 1. Execute the very first shot immediately and synchronously
        Fire(CameraLocation, CameraRotation);

        // 2. If the weapon is automatic, set a looping timer for subsequent shots
        if (WeaponData.bAutomaticFire && WeaponData.RateOfFire > 0.0f)
        {
            GetWorldTimerManager().SetTimer(
                ShotDelay, 
                this, 
                &AWeaponBase::AutomaticFireTimerCallback, 
                (60.0f / WeaponData.RateOfFire), 
                true
            );
        }

        if (bShowDebug)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Started firing sequence"));
        }

        // Simultaneously begins to play the recoil timeline locally
        if (AFPSCharacter* PlayerCharacter = Cast<AFPSCharacter>(GetOwner()))
        {
            if (PlayerCharacter->IsLocallyControlled())
            {
                StartRecoil();
            }
        }
    }
}

void AWeaponBase::AutomaticFireTimerCallback()
{
    if (AFPSCharacter* PlayerChar = Cast<AFPSCharacter>(GetOwner()))
    {
        if (PlayerChar->GetCameraComponent())
        {
            FVector UpdatedLoc = PlayerChar->GetCameraComponent()->GetComponentLocation();
            FRotator UpdatedRot = PlayerChar->GetCameraComponent()->GetComponentRotation();
            
            // Execute the next automatic shot with real-time aim coordinates
            Fire(UpdatedLoc, UpdatedRot);
        }
    }
}

// Start Recoil

void AWeaponBase::StartRecoil()
{
    AFPSCharacter *PlayerCharacter = Cast<AFPSCharacter>(GetOwner());
    AFPSCharacterController *CharacterController = Cast<AFPSCharacterController>(PlayerCharacter->GetController());

    if (bCanFire && !bIsReloading && CharacterController)
    {
        // Plays the recoil timelines and saves the current control rotation in order to recover to it
        VerticalRecoilTimeline.PlayFromStart();
        HorizontalRecoilTimeline.PlayFromStart();
        ControlRotation = CharacterController->GetControlRotation();
        bShouldRecover = true;
    }
}

bool AWeaponBase::Client_StartRecoil_Validate()
{
    return true;
}

void AWeaponBase::Client_StartRecoil_Implementation()
{
    StartRecoil();
}

void AWeaponBase::EnableFire()
{
    // Fairly self explanatory - allows the weapon to fire again after waiting for an animation to finish or finishing a reload
    bCanFire = true;
}

void AWeaponBase::ReadyToFire()
{
    bIsWeaponReadyToFire = true;
}

void AWeaponBase::StopFire()
{
    // Stops the gun firing (for automatic fire)
    VerticalRecoilTimeline.Stop();
    HorizontalRecoilTimeline.Stop();
    
    if (AFPSCharacter* PlayerChar = Cast<AFPSCharacter>(GetOwner()))
    {
        if (PlayerChar->IsLocallyControlled())
        {
            RecoilRecovery();
        }
    }
    ShotsFired = 0;

    if (WeaponData.bPreventRapidManualFire && bHasFiredRecently)
    {
        bHasFiredRecently = false;
        bIsWeaponReadyToFire = false;
        GetWorldTimerManager().ClearTimer(SpamFirePreventionDelay);
        GetWorldTimerManager().SetTimer(SpamFirePreventionDelay, this, &AWeaponBase::ReadyToFire, GetWorldTimerManager().GetTimerRemaining(ShotDelay), false, GetWorldTimerManager().GetTimerRemaining(ShotDelay));
    }
    GetWorldTimerManager().ClearTimer(ShotDelay);
}

bool AWeaponBase::Client_StopFire_Validate()
{
    return true;
}

void AWeaponBase::Client_StopFire_Implementation()
{
    StopFire();
}

void AWeaponBase::Fire(FVector CameraLocation, FRotator CameraRotation)
{
    // Allowing the gun to fire if it has ammunition, is not reloading and the bCanFire variable is true
    if (bCanFire && bIsWeaponReadyToFire && GeneralWeaponData.ClipSize > 0 && !bIsReloading)
    {
        // Casting to the player character
        AFPSCharacter *PlayerCharacter = Cast<AFPSCharacter>(GetOwner());

        // Printing debug strings
        if (bShowDebug)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "Fire", true);
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::FromInt(GeneralWeaponData.ClipSize > 0 && !bIsReloading), true);
        }

        // Subtracting from the ammunition count of the weapon
        GeneralWeaponData.ClipSize -= 1;

        const int NumberOfShots = WeaponData.bIsShotgun ? WeaponData.ShotgunPellets : 1;
        // We run this for the number of bullets/projectiles per shot, in order to support shotguns

        for (int i = 0; i < NumberOfShots; i++)
        {

            // Calculating the start and end points of our line trace, and applying randomised variation
            TraceStart = CameraLocation;
            TraceStartRotation = CameraRotation;

            float AccuracyMultiplier = 1.0f;
            if (PlayerCharacter->GetMovementState() == EMovementState::State_Sprint)
            {
                AccuracyMultiplier = WeaponData.AccuracyDebuff;
            }

            TraceStartRotation.Pitch += FMath::FRandRange(-((WeaponData.WeaponPitchVariation + WeaponPitchModifier) * AccuracyMultiplier), (WeaponData.WeaponPitchVariation + WeaponPitchModifier) * AccuracyMultiplier);
            TraceStartRotation.Yaw += FMath::FRandRange(-((WeaponData.WeaponYawVariation + WeaponYawModifier) * AccuracyMultiplier), (WeaponData.WeaponYawVariation + WeaponYawModifier) * AccuracyMultiplier);
            TraceDirection = TraceStartRotation.Vector();
            TraceEnd = TraceStart + (TraceDirection * (WeaponData.bIsShotgun ? WeaponData.ShotgunRange : WeaponData.LengthMultiplier));

            // Applying Recoil to the weapon locally
            if (PlayerCharacter->IsLocallyControlled())
            {
                Recoil();
            }

            EndPoint = TraceEnd;

            // Sets the default values for our trace query
            QueryParams.AddIgnoredActor(this);
            QueryParams.AddIgnoredActor(PlayerCharacter);
            // Must be false to hit the Physics Asset capsules. True attempts per-poly collision, which Skeletal Meshes ignore.
            QueryParams.bTraceComplex = false;
            QueryParams.bReturnPhysicalMaterial = true;

            // Drawing a line trace based on the parameters calculated previously
            if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_GameTraceChannel1, QueryParams))
            {
                // Drawing debug line trace
                if (bShowDebug)
                {
                    USkeletalMeshComponent* TraceMesh = (WeaponData.bHasAttachments && BarrelAttachment->DoesSocketExist(WeaponData.MuzzleLocation)) ? BarrelAttachment : MeshComp;

                    // Debug line from muzzle to hit location
                    DrawDebugLine(
                        GetWorld(), TraceMesh->GetSocketLocation(WeaponData.MuzzleLocation), Hit.Location,
                        FColor::Red, false, 10.0f, 0.0f, 2.0f);

                    if (bDrawObstructiveDebugs)
                    {
                        // Debug line from camera to hit location
                        DrawDebugLine(GetWorld(), TraceStart, Hit.Location, FColor::Orange, false, 10.0f, 0.0f, 2.0f);

                        // Debug line from camera to target location
                        DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green, false, 10.0f, 0.0f, 2.0f);
                    }
                }

                // Resetting finalDamage
                FinalDamage = 0.0f;

                // Setting finalDamage based on the type of surface hit
                FinalDamage = (WeaponData.BaseDamage + DamageModifier);

                if (Hit.PhysMaterial.Get() == WeaponData.HeadshotDamageSurface)
                {
                    FinalDamage = (WeaponData.BaseDamage + DamageModifier) * WeaponData.HeadshotMultiplier;
                }

                AActor *HitActor = Hit.GetActor();

                // Applying the previously set damage to the hit actor
                UGameplayStatics::ApplyPointDamage(HitActor, FinalDamage, TraceDirection, Hit, GetOwner()->GetInstigatorController(), this, DamageType);

                EndPoint = Hit.Location;

                // Passing hit delegate to InventoryComponent
                AFPSCharacter *PlayerRef = Cast<AFPSCharacter>(GetOwner());
                if (PlayerRef)
                {
                    UInventoryComponent *PlayerInventoryComp = PlayerRef->FindComponentByClass<UInventoryComponent>();
                    if (IsValid(PlayerInventoryComp))
                    {
                        PlayerInventoryComp->EventHitActor.Broadcast(Hit);
                    }
                }
            }
            else
            {
                // Drawing debug line trace
                if (bShowDebug)
                {
                    USkeletalMeshComponent* TraceMesh = (WeaponData.bHasAttachments && BarrelAttachment->DoesSocketExist(WeaponData.MuzzleLocation)) ? BarrelAttachment : MeshComp;

                    DrawDebugLine(
                        GetWorld(), TraceMesh->GetSocketLocation(WeaponData.MuzzleLocation), TraceEnd,
                        FColor::Red, false, 10.0f, 0.0f, 2.0f);

                    if (bDrawObstructiveDebugs)
                    {
                        // Debug line from camera to target location
                        DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green, false, 10.0f, 0.0f, 2.0f);
                    }
                }
            }
            if (HasAuthority()) { Multi_Fire(Hit); } else { PlayHitVFX(Hit); }
        }
        if (HasAuthority()) { Multi_FireOnce(); } else { PlayFireVFX(); }
        
        if (!WeaponData.bAutomaticFire)
        {
            VerticalRecoilTimeline.Stop();
            HorizontalRecoilTimeline.Stop();
            if (PlayerCharacter->IsLocallyControlled())
            {
                RecoilRecovery();
            }
        }

        if (!WeaponData.bIsShotgun)
        {
            if (WeaponData.Gun_Shot)
            {
                if (WeaponData.bWaitForAnim)
                {
                    // Preventing the player from firing the weapon until the animation finishes playing
                    const float AnimWaitTime = WeaponData.Gun_Shot->GetPlayLength();
                    bCanFire = false;
                    // Reset the timer handle
                    GetWorldTimerManager().ClearTimer(AnimationWaitDelay);
                    GetWorldTimerManager().SetTimer(AnimationWaitDelay, this, &AWeaponBase::EnableFire, AnimWaitTime, false, AnimWaitTime);
                }
            }
        }
        else
        {
            if (WeaponData.Gun_Shot)
            {
                if (!ShotGunFiredFirstShot)
                {
                    if (WeaponData.bWaitForAnim)
                    {
                        // Preventing the player from firing the weapon until the animation finishes playing
                        const float AnimWaitTime = WeaponData.Gun_Shot->GetPlayLength();
                        bCanFire = false;
                        // Reset the timer handle
                        GetWorldTimerManager().ClearTimer(AnimationWaitDelay);
                        GetWorldTimerManager().SetTimer(AnimationWaitDelay, this, &AWeaponBase::EnableFire, AnimWaitTime, false, AnimWaitTime);
                    }
                    ShotGunFiredFirstShot = true;
                }
                else
                {
                    if (WeaponData.bWaitForAnim)
                    {
                        // Preventing the player from firing the weapon until the animation finishes playing
                        const float AnimWaitTime = WeaponData.ShotGun_Shot2->GetPlayLength();
                        bCanFire = false;
                        // Reset the timer handle
                        GetWorldTimerManager().ClearTimer(AnimationWaitDelay);
                        GetWorldTimerManager().SetTimer(AnimationWaitDelay, this, &AWeaponBase::EnableFire, AnimWaitTime, false, AnimWaitTime);
                    }
                    ShotGunFiredFirstShot = false;
                }
            }
        }
        bHasFiredRecently = true;
    }
    else if (bCanFire && !bIsReloading)
    {
        if (HasAuthority()) { Multi_Fire_NoBullets(); } else { PlayNoBulletsVFX(); }
    }
}

bool AWeaponBase::Multi_Fire_Validate(FHitResult HitResult)
{
    return true;
}
void AWeaponBase::Multi_Fire_Implementation(FHitResult HitResult)
{
    AFPSCharacter* Char = Cast<AFPSCharacter>(GetOwner());
    if (Char && Char->IsLocallyControlled() && !HasAuthority()) return; // Prevent double-VFX on predicting Client
    PlayHitVFX(HitResult);
}

void AWeaponBase::PlayHitVFX(FHitResult HitResult)
{
    FRotator EjectionSpawnVector = FRotator::ZeroRotator;
    EjectionSpawnVector.Yaw = 270.0f;
    UNiagaraFunctionLibrary::SpawnSystemAttached(EjectedCasing, MagazineAttachment, FName("ejection_port"), FVector::ZeroVector, EjectionSpawnVector, EAttachLocation::SnapToTarget, true, true);

    EndPoint = HitResult.Location;

    USkeletalMeshComponent* TargetMuzzleMesh = MeshComp;
    if (AFPSCharacter* PlayerCharacter = Cast<AFPSCharacter>(GetOwner()))
    {
        if (PlayerCharacter->IsLocallyControlled())
        {
            TargetMuzzleMesh = (WeaponData.bHasAttachments && BarrelAttachment->DoesSocketExist(WeaponData.ParticleSpawnLocation)) ? BarrelAttachment : MeshComp;
        }
        else
        {
            TargetMuzzleMesh = TPMeshComp;
        }
    }

    const FRotator ParticleRotation = (EndPoint - TargetMuzzleMesh->GetSocketLocation(WeaponData.MuzzleLocation)).Rotation();

    // Spawns the bullet trace properly, ignoring if null in the datatable
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(), 
        WeaponData.BulletTrace, 
        TargetMuzzleMesh->GetSocketLocation(WeaponData.ParticleSpawnLocation), 
        ParticleRotation
    );

    // Selecting the hit effect based on the hit physical surface material (hit.PhysMaterial.Get()) and spawning it (Niagara)

    if (HitResult.PhysMaterial.Get() == WeaponData.NormalDamageSurface || HitResult.PhysMaterial.Get() == WeaponData.HeadshotDamageSurface)
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponData.EnemyHitEffect, HitResult.GetComponent(), "", HitResult.ImpactPoint, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition, false);
    }

    else if (HitResult.PhysMaterial.Get() == WeaponData.GroundSurface)
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponData.GroundHitEffect, HitResult.GetComponent(), "", HitResult.ImpactPoint, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition, false);
    }
    else if (HitResult.PhysMaterial.Get() == WeaponData.RockSurface)
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponData.RockHitEffect, HitResult.GetComponent(), "", HitResult.ImpactPoint, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition, false);
    }
    else
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponData.DefaultHitEffect, HitResult.GetComponent(), "", HitResult.ImpactPoint, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition, false);
    }
}

bool AWeaponBase::Multi_FireOnce_Validate()
{
    return true;
}
void AWeaponBase::Multi_FireOnce_Implementation()
{
    AFPSCharacter* Char = Cast<AFPSCharacter>(GetOwner());
    if (Char && Char->IsLocallyControlled() && !HasAuthority()) return; // Prevent double-VFX on predicting Client
    PlayFireVFX();
}

void AWeaponBase::PlayFireVFX()
{
    // Playing an animation on the weapon mesh
    if (!WeaponData.bIsShotgun)
    {
        if (WeaponData.Gun_Shot)
        {
            MeshComp->PlayAnimation(WeaponData.Gun_Shot, false);
            TPMeshComp->PlayAnimation(WeaponData.Gun_Shot, false);
        }
    }
    else
    {
        if (WeaponData.Gun_Shot)
        {
            if (!ShotGunFiredFirstShot)
            {
                MeshComp->PlayAnimation(WeaponData.Gun_Shot, false);
                TPMeshComp->PlayAnimation(WeaponData.Gun_Shot, false);
                ShotGunFiredFirstShot = true;
            }
            else
            {
                MeshComp->PlayAnimation(WeaponData.ShotGun_Shot2, false);
                TPMeshComp->PlayAnimation(WeaponData.ShotGun_Shot2, false);
                ShotGunFiredFirstShot = false;
            }
        }
    }

    if (AFPSCharacter* PlayerCharacter = Cast<AFPSCharacter>(GetOwner()))
    {
        UAnimMontage* TargetMontage = nullptr;

        if (PlayerCharacter->IsLocallyControlled())
        {
            TargetMontage = PlayerCharacter->IsPlayerAiming() && WeaponData.FP_Player_ADS_Shot ? WeaponData.FP_Player_ADS_Shot : WeaponData.FP_Player_Shot;
            if (TargetMontage && PlayerCharacter->GetHandsMesh()->GetAnimInstance())
            {
                AnimTime = PlayerCharacter->GetHandsMesh()->GetAnimInstance()->Montage_Play(TargetMontage, 1.0f);
            }
        }
        else
        {
            TargetMontage = PlayerCharacter->IsPlayerAiming() && WeaponData.TP_Player_ADS_Shot ? WeaponData.TP_Player_ADS_Shot : WeaponData.TP_Player_Shot;
            if (TargetMontage && PlayerCharacter->GetThirdPersonMesh()->GetAnimInstance())
            {
                // We do not overwrite AnimTime for simulated proxies to avoid timer desync on the local client
                PlayerCharacter->GetThirdPersonMesh()->GetAnimInstance()->Montage_Play(TargetMontage, 1.0f);
            }
        }
    }

    // Determines which mesh to attach the VFX to based on perspective
    USkeletalMeshComponent* TargetVFXMesh = MeshComp;

    if (AFPSCharacter* PlayerCharacter = Cast<AFPSCharacter>(GetOwner()))
    {
        if (PlayerCharacter->IsLocallyControlled())
        {
            TargetVFXMesh = (WeaponData.bHasAttachments && BarrelAttachment->DoesSocketExist(WeaponData.ParticleSpawnLocation)) ? BarrelAttachment : MeshComp;
        }
        else
        {
            TargetVFXMesh = TPMeshComp;
        }
    }

    if (TargetVFXMesh)
    {
        // Spawns the muzzle flash attached to the designated socket
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            WeaponData.MuzzleFlash,
            TargetVFXMesh,
            WeaponData.ParticleSpawnLocation,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true
        );
    }

    // Spawning the firing sound
    if (WeaponData.bSilenced)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData.SilencedSound, MeshComp->GetSocketLocation(WeaponData.MuzzleLocation));
    }
    else
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData.FireSound, MeshComp->GetSocketLocation(WeaponData.MuzzleLocation));
    }
}

bool AWeaponBase::Multi_Fire_NoBullets_Validate()
{
    return true;
}
void AWeaponBase::Multi_Fire_NoBullets_Implementation()
{
    AFPSCharacter* Char = Cast<AFPSCharacter>(GetOwner());
    if (Char && Char->IsLocallyControlled() && !HasAuthority()) return; // Prevent double-VFX on predicting Client
    PlayNoBulletsVFX();
}

void AWeaponBase::PlayNoBulletsVFX()
{
    UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData.EmptyFireSound, MeshComp->GetSocketLocation(WeaponData.MuzzleLocation));
    // Clearing the ShotDelay timer so that we don't have a constant ticking when the player has no ammo, just a single click
    GetWorldTimerManager().ClearTimer(ShotDelay);
}

void AWeaponBase::Recoil()
{
    AFPSCharacter *PlayerCharacter = Cast<AFPSCharacter>(GetOwner());
    AFPSCharacterController *CharacterController = Cast<AFPSCharacterController>(PlayerCharacter->GetController());

    // Apply recoil by adding a pitch and yaw input to the character controller
    if (WeaponData.bAutomaticFire && CharacterController && ShotsFired > 0 && IsValid(WeaponData.VerticalRecoilCurve) && IsValid(WeaponData.HorizontalRecoilCurve))
    {
        CharacterController->AddPitchInput(WeaponData.VerticalRecoilCurve->GetFloatValue(VerticalRecoilTimeline.GetPlaybackPosition()) * VerticalRecoilModifier);
        CharacterController->AddYawInput(WeaponData.HorizontalRecoilCurve->GetFloatValue(HorizontalRecoilTimeline.GetPlaybackPosition()) * HorizontalRecoilModifier);
    }
    else if (CharacterController && ShotsFired <= 0 && IsValid(WeaponData.VerticalRecoilCurve) && IsValid(WeaponData.HorizontalRecoilCurve))
    {
        CharacterController->AddPitchInput(WeaponData.VerticalRecoilCurve->GetFloatValue(0) * VerticalRecoilModifier);
        CharacterController->AddYawInput(WeaponData.HorizontalRecoilCurve->GetFloatValue(0) * HorizontalRecoilModifier);
    }

    ShotsFired += 1;
    if (CharacterController)
    {
        CharacterController->ClientStartCameraShake(WeaponData.RecoilCameraShake);
    }
}

bool AWeaponBase::Client_Recoil_Validate()
{
    return true;
}

void AWeaponBase::Client_Recoil_Implementation()
{
    Recoil();
}

void AWeaponBase::RecoilRecovery()
{
    // Plays the recovery timeline
    if (bShouldRecover)
    {
        RecoilRecoveryTimeline.PlayFromStart();
    }
}

bool AWeaponBase::Client_RecoilRecovery_Validate()
{
    return true;
}

void AWeaponBase::Client_RecoilRecovery_Implementation()
{
    RecoilRecovery();
}

void AWeaponBase::CancelReload()
{
	if (bIsReloading)
	{
		bIsReloading = false;
		GetWorldTimerManager().ClearTimer(ReloadingDelay);
		bCanFire = true;
		
		if (MeshComp && MeshComp->GetAnimInstance()) MeshComp->GetAnimInstance()->StopAllMontages(0.0f);
		if (TPMeshComp && TPMeshComp->GetAnimInstance()) TPMeshComp->GetAnimInstance()->StopAllMontages(0.0f);
	}
}

bool AWeaponBase::Reload()
{
    if (!bCanReload)
    {
        return false;
    }
    // Changing the maximum ammunition based on if the weapon can hold a bullet in the chamber
    int Value = 0;
    if (WeaponData.bCanBeChambered)
    {
        Value = 1;
    }

    // Casting to the character controller (which stores all the ammunition and health variables)
    AFPSCharacter *PlayerCharacter = Cast<AFPSCharacter>(GetOwner());
    if (!PlayerCharacter) return false;

    if (!bIsReloading && (GeneralWeaponData.ClipSize < (GeneralWeaponData.ClipCapacity + Value)))
    {
        Multi_Reload();
        UAnimMontage* TargetMontage = nullptr;
        bool bIsEmpty = GeneralWeaponData.ClipSize <= 0;

        if (PlayerCharacter && PlayerCharacter->IsLocallyControlled())
        {
            TargetMontage = bIsEmpty ? WeaponData.FP_EmptyPlayerReload : WeaponData.FP_PlayerReload;
        }
        else
        {
            TargetMontage = bIsEmpty ? WeaponData.TP_EmptyPlayerReload : WeaponData.TP_PlayerReload;
        }

        if (TargetMontage)
        {
            AnimTime = TargetMontage->GetPlayLength();
        }
        else
        {
            AnimTime = 2.0f;
        }

        if (bShowDebug)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "Reload", true);
        }

        bCanFire = false;
        bIsReloading = true;

        GetWorldTimerManager().SetTimer(ReloadingDelay, this, &AWeaponBase::UpdateAmmo, AnimTime, false, AnimTime);
    }
    return true;
}

bool AWeaponBase::Multi_Reload_Validate()
{
    return true;
}

void AWeaponBase::Multi_Reload_Implementation()
{
    bCanFire = false;
	bIsReloading = true;

    AFPSCharacter *PlayerCharacter = Cast<AFPSCharacter>(GetOwner());
    if (!PlayerCharacter) return;

    bool bIsEmpty = GeneralWeaponData.ClipSize <= 0;

    if (bIsEmpty && WeaponData.EmptyWeaponReload)
    {
        // Force the base weapon mesh to play the animation.
        // The b_gun_mag bone will move, carrying the attached Magazine mesh with it.
        MeshComp->PlayAnimation(WeaponData.EmptyWeaponReload, false);
        TPMeshComp->PlayAnimation(WeaponData.EmptyWeaponReload, false);
    }
    else if (!bIsEmpty && WeaponData.WeaponReload)
    {
        MeshComp->PlayAnimation(WeaponData.WeaponReload, false);
        TPMeshComp->PlayAnimation(WeaponData.WeaponReload, false);
    }

    if (PlayerCharacter->IsLocallyControlled())
    {
        UAnimMontage* TargetMontage = bIsEmpty ? WeaponData.FP_EmptyPlayerReload : WeaponData.FP_PlayerReload;
        if (TargetMontage && PlayerCharacter->GetHandsMesh()->GetAnimInstance())
        {
            PlayerCharacter->GetHandsMesh()->GetAnimInstance()->Montage_Play(TargetMontage, 1.0f);

            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &AWeaponBase::OnLocalReloadEnded);
            PlayerCharacter->GetHandsMesh()->GetAnimInstance()->Montage_SetEndDelegate(EndDelegate, TargetMontage);
        }
    }
    else
    {
        UAnimMontage* TargetMontage = bIsEmpty ? WeaponData.TP_EmptyPlayerReload : WeaponData.TP_PlayerReload;
        if (TargetMontage && PlayerCharacter->GetThirdPersonMesh()->GetAnimInstance())
        {
            PlayerCharacter->GetThirdPersonMesh()->GetAnimInstance()->Montage_Play(TargetMontage, 1.0f);
        }
    }
}

void AWeaponBase::Multi_SwapWeaponAnim_Implementation()
{
    if (AFPSCharacter *FPSCharacter = Cast<AFPSCharacter>(GetOwner()))
    {
        if (FPSCharacter->IsLocallyControlled() && GetStaticWeaponData()->FP_WeaponEquip)
        {
            if (FPSCharacter->GetHandsMesh()->GetAnimInstance())
            {
                FPSCharacter->GetHandsMesh()->GetAnimInstance()->Montage_Play(GetStaticWeaponData()->FP_WeaponEquip, 1.0f);
            }
        }
        else if (!FPSCharacter->IsLocallyControlled() && GetStaticWeaponData()->TP_WeaponEquip)
        {
            if (FPSCharacter->GetThirdPersonMesh()->GetAnimInstance())
            {
                FPSCharacter->GetThirdPersonMesh()->GetAnimInstance()->Montage_Play(GetStaticWeaponData()->TP_WeaponEquip, 1.0f);
            }
        }
    }
}

void AWeaponBase::Multi_UnequipWeaponAnim_Implementation()
{
    if (AFPSCharacter *FPSCharacter = Cast<AFPSCharacter>(GetOwner()))
    {
        if (FPSCharacter->IsLocallyControlled())
        {
            return;
        }
        if (GetStaticWeaponData()->TP_WeaponUnequip && FPSCharacter->GetThirdPersonMesh()->GetAnimInstance())
        {
            FPSCharacter->GetThirdPersonMesh()->GetAnimInstance()->Montage_Play(GetStaticWeaponData()->TP_WeaponUnequip, 1.0f);
        }
    }
}

void AWeaponBase::UpdateAmmo()
{
    if (bShowDebug)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "UpdateAmmo", true);
    }

    AFPSCharacter *PlayerCharacter = Cast<AFPSCharacter>(GetOwner());

    int Value = 0;
    if (GeneralWeaponData.ClipSize > 0 && WeaponData.bCanBeChambered)
    {
        Value = 1;
        if (bShowDebug) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Value = 1", true);
    }

    GeneralWeaponData.ClipSize = GeneralWeaponData.ClipCapacity + Value;

    if (bShowDebug)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, FString::FromInt(GeneralWeaponData.ClipSize), true);
    }

    // Resetting bIsReloading and allowing the player to fire the gun again
    bIsReloading = false;

    // Lift the sprint restriction on the character's owning Client
    if (PlayerCharacter)
    {
        PlayerCharacter->Client_CompleteReload();
    }

    // Making sure the player cannot fire if sliding
    if (!(PlayerCharacter->GetMovementState() == EMovementState::State_Slide))
    {
        EnableFire();
    }

    // Setting weapon animation after reload
    MeshComp->PlayAnimation(WeaponData.WeaponIdle, false);
    TPMeshComp->PlayAnimation(WeaponData.WeaponIdle, false);

    bIsWeaponReadyToFire = true;
}

// Called every frame
void AWeaponBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    VerticalRecoilTimeline.TickTimeline(DeltaTime);
    HorizontalRecoilTimeline.TickTimeline(DeltaTime);
    RecoilRecoveryTimeline.TickTimeline(DeltaTime);

    // Ensure consistent attachment for both Client and Server, fixing initialization race conditions
    if (AFPSCharacter* CurrentPlayer = Cast<AFPSCharacter>(GetOwner()))
    {
        // 1. Force Third Person Attachment (Legacy brute-force behavior)
        TPMeshComp->AttachToComponent(CurrentPlayer->GetThirdPersonMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetStaticWeaponData()->TP_WeaponAttachmentSocketName);

        // 2. Intelligently force First Person Attachment if it failed during BeginPlay/OnRep_Owner
        if (MeshComp->GetAttachParent() == nullptr || MeshComp->GetAttachParent() != CurrentPlayer->GetHandsMesh())
        {
            MeshComp->AttachToComponent(CurrentPlayer->GetHandsMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetStaticWeaponData()->FP_WeaponAttachmentSocketName);
        }
    }

    if (bShowDebug)
    {
        GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Green, bHasFiredRecently ? TEXT("Has fired recently") : TEXT("Has not fired recently"));
        GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Green, bCanFire ? TEXT("Can Fire") : TEXT("Can not Fire"));
        GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Green, bIsWeaponReadyToFire ? TEXT("Weapon is ready to fire") : TEXT("Weapon is not ready to fire"));
    }
}

// Recovering the player's recoil to the pre-fired position
void AWeaponBase::HandleRecoveryProgress(float Value) const
{
    // Getting a reference to the Character Controller
    AFPSCharacter *PlayerCharacter = Cast<AFPSCharacter>(GetOwner());
    AFPSCharacterController *CharacterController = Cast<AFPSCharacterController>(PlayerCharacter->GetController());

    // Calculating the new control rotation by interpolating between current and target
    const FRotator NewControlRotation = FMath::Lerp(CharacterController->GetControlRotation(), ControlRotation, Value);

    CharacterController->SetControlRotation(NewControlRotation);
}

bool AWeaponBase::Client_HandleRecoveryProgress_Validate(float Value) const
{
    return true;
}

void AWeaponBase::Client_HandleRecoveryProgress_Implementation(float Value) const
{
    HandleRecoveryProgress(Value);
}

void AWeaponBase::OnLocalReloadEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (bInterrupted)
    {
        CancelReload();
        if (!HasAuthority())
        {
            Server_CancelReload();
        }
    }
}

void AWeaponBase::Server_CancelReload_Implementation()
{
    CancelReload();
}