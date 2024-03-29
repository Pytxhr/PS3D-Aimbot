void *me = nullptr;
void *enemy = nullptr;
void* closestEnemy;
bool isAimbot;
float maxRangeSquared = 2500.0f; // Maximum range squared

void *(*Component_get_transform)(void*);
Vector3 (*Transform_get_position)(void*);
bool (*Fire)(void *);
void (*Transform_Set_Rotation)(void* transform, Quaternion pos);
Quaternion (*Transform_Get_Rotation)(void* transform);
float (*smoothDeltaTime)(void*);


/*
 * Function to check if player is Mine.
 */
bool get_isMine(void *Player){
    if(Player != nullptr) {
        auto netView = *(void **)((uint64_t)Player + 0x110);
        if(netView != nullptr){
            return *(bool *)((uint64_t)netView + 0x90);
        }
    }
    return false;
}

/*
 * Function to check if a player is living (i.e., not killed or dead).
 */
bool IsEnemyKilled(void* EnemyPlayer) {
    if (EnemyPlayer != nullptr) {
        auto health = *(void**)((uint64_t)EnemyPlayer + 0xC8);
        if (health != nullptr) {
            return *(bool *)((uint64_t)health + 0xD0);
        }
    }
    return false;
}

// Function to check if the player is shooting
bool IsPlayerShooting(void* MyPlayer) {
    if (MyPlayer != nullptr) {
        auto WeaponController = *(void**)((uint64_t)MyPlayer + 0x50);
        if (WeaponController != nullptr) {
            if(Fire != nullptr)
            {
                return Fire(WeaponController);
            }
        }
    }
    return false;
}


/*
 * Function to calculate squared distance between two positions.
 */
float CalculateSquaredDistance(Vector3 position1, Vector3 position2) {
    float dx = position2.X - position1.X;
    float dy = position2.Y - position1.Y;
    float dz = position2.Z - position1.Z;
    return dx * dx + dy * dy + dz * dz;
}

/*
 * Function to check if the enemy is within a specified range from the player.
 */
bool IsEnemyWithinRange(void *MyPlayer, void *EnemyPlayer, float range) {
    Vector3 enemyPosition = Transform_get_position(Component_get_transform(EnemyPlayer));
    Vector3 myPosition = Transform_get_position(Component_get_transform(MyPlayer));
    float distance = Vector3::Distance(enemyPosition, myPosition);
    return distance <= range;
}

/*
 * Function to update the pointer to the closest enemy based on squared distance calculation.
 */
void GetClosestEnemyPosition(void* MyPlayer, void* EnemyPlayer) {
    if (!IsEnemyKilled(EnemyPlayer)) { // Check if the enemy player is not alive

        Vector3 enemyPosition = Transform_get_position(Component_get_transform(EnemyPlayer));
        Vector3 myPosition = Transform_get_position(Component_get_transform(MyPlayer));

        float squaredDistance = CalculateSquaredDistance(enemyPosition, myPosition);

        if (squaredDistance < maxRangeSquared) {
            maxRangeSquared = squaredDistance;
            closestEnemy = EnemyPlayer;
        }
    }
}


/*
 * Function to aim towards the closest enemy within range if the aimbot is active and the player is shooting.
 * This function updates the player's rotation to aim at the closest enemy within a specified range.
 *
 * Parameters:
 * - MyPlayer: Pointer to the player entity.
 * - EnemyPlayer: Pointer to the enemy entity.
 * - smoothDeltaTime: Smooth delta time for rotation interpolation.
 */
void AimTowardsClosestEnemy(void *MyPlayer, void *EnemyPlayer, float SDT) {
    // Check if the enemy is within a larger range for potential targeting
    if (IsEnemyWithinRange(MyPlayer, EnemyPlayer, 50.0f)) {
        // Update the pointer to the closest enemy
        GetClosestEnemyPosition(MyPlayer, EnemyPlayer);

        // Check if a closest enemy is found and if the player is shooting
        if (closestEnemy != nullptr && IsPlayerShooting(MyPlayer)) {
            // Get positions of the player and the closest enemy
            Vector3 enemyPosition = Transform_get_position(Component_get_transform(closestEnemy));
            Vector3 myPosition = Transform_get_position(Component_get_transform(MyPlayer));

            // Calculate the initial distance to the target
            float distance = Vector3::Distance(enemyPosition, myPosition);

            if (distance < 30) {
                // Calculate the direction to the target enemy
                Vector3 directionToTarget = Vector3::Normalized(enemyPosition - myPosition);

                // Calculate the target rotation to aim at the enemy
                Quaternion targetRotation = Quaternion::LookRotation(directionToTarget);

                // Get the current rotation of our player
                Quaternion currentRotation = Transform_Get_Rotation(Component_get_transform(MyPlayer));

                // Interpolate smoothly between the current rotation and the target rotation
                Quaternion smoothedRotation = Quaternion::Slerp(currentRotation, targetRotation, SDT * 50.0f);

                // Set our player's rotation to the smoothed rotation
                Transform_Set_Rotation(Component_get_transform(MyPlayer), smoothedRotation);

            }
        }
    }
}

/*
 * Function to update game objects and perform aimbot functionality during gameplay.
 */
void( *old_NetworkPlayer_Update)(void * obj);
void NetworkPlayer_Update(void * obj) {
    if (obj != nullptr) {

        float SDT = smoothDeltaTime(obj);

        // LOGD("myPos: x = ", myPos.X, ", y = ", myPos.Y, ", z = ", myPos.Z);
        // LOGD("enemyPos: x = ", enemyPos.X, ", y = ", enemyPos.Y, ", z = ", enemyPos.Z);

        // Determine if the object is the player or the enemy
        if (get_isMine(obj)) {
            me = obj; // Set the player object
        } else {
            enemy = obj; // Set the enemy object
        }

        if (isAimbot) {
            AimTowardsClosestEnemy(me, enemy, SDT);
        }

    } 
    old_NetworkPlayer_Update(obj);
}


void *hack_thread(void *) {
    using namespace BNM;
    LOGI(OBFUSCATE("pthread created"));

    do {
        sleep(1);
    } while (!Il2cppLoaded());
    AttachIl2Cpp();
    LOGI(OBFUSCATE("%s has been loaded"), (const char *) targetLibName);
#if defined(__aarch64__)
#define BNM_USE_APPDOMAIN
    DobbyHook((void*)getRealOffset(0xF06C14), (void*) NetworkPlayer_Update, (void**)&old_NetworkPlayer_Update);


    Component_get_transform = (void* (*)(void*))getRealOffset(0x2FEFCBC);
    Transform_get_position = (Vector3 (*)(void*))getRealOffset(0x2FF8700);
    Transform_Set_Rotation = (void (*)(void*, Quaternion))getRealOffset(0x2FF8C28);
    Transform_Get_Rotation= (Quaternion (*)(void*))getRealOffset(0x2FF8BA0);
    smoothDeltaTime = (float (*)(void*))getRealOffset(0x2FE528C);
    Fire = (bool (*)(void *))getRealOffset(0xF212A4);


#else
    LOGI(OBFUSCATE("Done"));
#endif
    DetachIl2Cpp();
    return nullptr;
}


