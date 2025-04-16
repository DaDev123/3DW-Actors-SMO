#include "actors/TuccondorFur.hpp"
#include "rs/util.hpp"
#include "al/util.hpp"
#include "game/Player/PlayerHitPointData.h"

namespace ca {

// TuccondorFur *tuccondorInstance;

TuccondorFur::TuccondorFur(const char *name) : al::LiveActor(name) { }

typedef void (TuccondorFur::*functorType)();

void TuccondorFur::init(al::ActorInitInfo const &info)
{
    al::initActorWithArchiveName(this, info, "TuccondorFur", nullptr);

    // Start in wait state
    al::initNerve(this, &nrvTuccondorFurWait, 1);

    this->state = new EnemyStateWander(this, "Walk");

    al::initNerveState(this, this->state, &nrvTuccondorFurWander, "徘徊");

    this->forceKeeper = new ExternalForceKeeper();

    this->makeActorAlive();

    al::setSensorRadius(this, "Explosion", 0.0f);
    al::setSensorRadius(this, "ExplosionToPlayer", 0.0f);

    al::invalidateHitSensor(this, "Explosion");
    al::invalidateHitSensor(this, "ExplosionToPlayer");

    ca::tuccondorInstance = this; 
}

void TuccondorFur::listenAppear() {
    this->appear();
}

bool TuccondorFur::receiveMsg(const al::SensorMsg* message, al::HitSensor* source,
                        al::HitSensor* target) {
    if(rs::isMsgTargetMarkerPosition(message)) {
        sead::Vector3f &transPtr = al::getTrans(this);
        rs::setMsgTargetMarkerPosition(message, sead::Vector3f(transPtr.x + 0.0, transPtr.y + 180.0f, transPtr.z + 0.0));
        return true;
    }

    if (rs::isMsgKillByShineGet(message)) {
        this->kill();
        return true;
    }

    if (rs::isMsgNpcScareByEnemy(message) || al::tryReceiveMsgPushAndAddVelocity(this, message, source, target, 1.0)) {
        // Zero velocity after being pushed
        al::setVelocityZero(this);
        return true;
    }

    if(rs::isMsgCapReflect(message) && !al::isNerve(this, &nrvTuccondorFurBlowDown) && this->capHitCooldown <= 0) {
        rs::requestHitReactionToAttacker(message, target, source);
        // Skip the CapHit state and go directly to Attack
        al::setNerve(this, &nrvTuccondorFurAttack);
        this->capPos = *al::getSensorPos(source);
        this->capHitCooldown = 10;
        return true;
    }

    if((rs::isMsgBlowDown(message) || rs::isMsgDonsukeAttack(message)) && !al::isNerve(this, &nrvTuccondorFurBlowDown)) {
        // Don't add velocity on blow attacks
        rs::setAppearItemFactorAndOffsetByMsg(this, message, source);
        rs::requestHitReactionToAttacker(message, target, source);
        return true;
    }

    if(!rs::isMsgPechoSpot(message) && !rs::isMsgDamageBallAttack(message) && !al::isMsgPlayerFireBallAttack(message) || !al::isSensorEnemyBody(target)) {
        // Don't let force keeper modify our position
        return false;
    }

    rs::setAppearItemFactorAndOffsetByMsg(this, message, source);
    rs::requestHitReactionToAttacker(message, target, source);
    al::startHitReaction(this, "死亡");
    this->kill();
    return false;
}

void TuccondorFur::attackSensor(al::HitSensor* source, al::HitSensor* target) {
    if(al::isNerve(this, &nrvTuccondorFurExplode)) {
        if(al::isSensorPlayer(source)) {
            if(al::isSensorName(target, "ExplosionToPlayer")) {
                al::sendMsgExplosion(target, source, nullptr);
            }
        }else {
            if(al::isSensorName(target, "Explosion")) {
                al::sendMsgExplosion(target, source, nullptr);
            }
        }
    }
    else {
        if (al::isSensorPlayer(source)) {
            rs::sendMsgPushToPlayer(target, source);
            // When player contacts sensor, start the action sequence
            if (al::isNerve(this, &nrvTuccondorFurWait)) {
                al::setNerve(this, &nrvTuccondorFurFind);
            }
            return;
        }
        else if (!al::sendMsgEnemyAttack(target, source)) {
            rs::sendMsgPushToPlayer(target, source);
            return;
        }
    }
}

// Override control function to prevent any movement
void TuccondorFur::control() {
    // Reset velocity to zero each frame to prevent any movement
    al::setVelocityZero(this);
    
    if (al::isInDeathArea(this) || al::isCollidedFloorCode(this, "DamageFire") ||
        al::isCollidedFloorCode(this, "Needle") || al::isCollidedFloorCode(this, "Poison") ||
        al::isInWaterArea(this)) {
        al::tryEmitEffect(this, "Explosion", nullptr);
        al::setEffectAllScale(this, "Explosion", sead::Vector3f(2.0f,2.0f,2.0f));
        this->kill();
    } else {
        // Decrement cap hit cooldown if needed
        int unk = this->capHitCooldown - 1;
        if(unk >= 0) {
            this->capHitCooldown = unk;
        }
        
        // Zero out future position to prevent drift
        this->futurePos = sead::Vector3f::zero;
        
        // Reset force keeper
        this->forceKeeper->reset();
        
        if (al::isNerve(this, &nrvTuccondorFurWait) && al::isNearPlayer(this, 1000.0f)) {
            // Start the sequence
            al::setNerve(this, &nrvTuccondorFurFind);
        }
        
        // Handle explosion timer if needed
        if(this->explodeTimer > 0) {
            this->explodeTimer--;
            if(this->explodeTimer == 0) {
                al::setNerve(this, &nrvTuccondorFurExplode);
            }
        }
    }
}

// Override updateCollider to prevent any movement from collisions
void TuccondorFur::updateCollider() {
    // Zero out velocity to prevent any movement
    al::setVelocityZero(this);
    
    if (al::isNoCollide(this)) {
        al::getActorCollider(this)->onInvalidate();
    } else {
        // Process collisions but don't move the actor
        al::getActorCollider(this)->collide(sead::Vector3f::zero);
    }
}

// Override updateVelocity to prevent any velocity changes
void TuccondorFur::updateVelocity() {
    // Zero out velocity to prevent any movement
    al::setVelocityZero(this);
}

void TuccondorFur::exeWait(void)  // 0x0
{
    if (al::isFirstStep(this)) {
        al::startAction(this, "Wait");
        al::setVelocityZero(this);
    }
    // Check if player is near while in Wait state
    if (al::isNearPlayer(this, 900.0f)) {
        al::setNerve(this, &nrvTuccondorFurFind);
    }

}

void TuccondorFur::exeWander(void)  // 0x8
{
    // Go back to wait state
    al::setNerve(this, &nrvTuccondorFurWait);
}

void TuccondorFur::exeCapHit(void)  // 0x10
{
    // Skip this state entirely and go back to wait
    al::setNerve(this, &nrvTuccondorFurWait);
}

void TuccondorFur::exeAttack(void)  // 0x20
{
    if (al::isFirstStep(this)) {
        al::startAction(this, "AttackSuccess");
        al::setVelocityZero(this);
    }

    if (al::isActionEnd(this)) {
        // Go back to wait state after the sequence is complete
        al::setNerve(this, &nrvTuccondorFurWait);
    }
}

void TuccondorFur::exeTurn(void)  // 0x28
{
    // Force transition back to wait state
    al::setNerve(this, &nrvTuccondorFurWait);
}

void TuccondorFur::exeFall(void)  // 0x30
{
    // Force transition back to wait state
    al::setNerve(this, &nrvTuccondorFurWait);
}

void TuccondorFur::exeFind(void)  // 0x38
{
    if (al::isFirstStep(this)) {
        al::setVelocityZero(this);
        al::startAction(this, "Find");
        
        // Look at player if possible
        PlayerActorHakoniwa* pActor = al::tryFindNearestPlayerActor(this);
        if(pActor) {
            al::turnToTarget(this, al::getTrans(pActor), 3.5f);
        }
    }

    if (al::isActionEnd(this)) {
        // Skip CapHit and go directly to Attack state
        al::setNerve(this, &nrvTuccondorFurAttack);
    }
}

void TuccondorFur::exeChase(void)  // 0x40
{
    // Force transition back to wait state
    al::setNerve(this, &nrvTuccondorFurWait);
}

void TuccondorFur::exeLand(void)  // 0x48
{
    // Force transition back to wait state
    al::setNerve(this, &nrvTuccondorFurWait);
}

void TuccondorFur::exeExplode(void) {
    if(al::isFirstStep(this)) {
        al::setVelocityZero(this);

        al::validateHitSensor(this, "Explosion");
        al::validateHitSensor(this, "ExplosionToPlayer");

        al::tryEmitEffect(this, "Explosion", nullptr);
        al::setEffectAllScale(this, "Explosion", sead::Vector3f(2.0f,2.0f,2.0f));
    }

    al::setSensorRadius(this, "Explosion", al::lerpValue(0.0f, 200.0f, al::calcNerveRate(this, 5)));
    al::setSensorRadius(this, "ExplosionToPlayer", al::lerpValue(0.0f, 100.0f, al::calcNerveRate(this, 5)));

    if(al::isGreaterEqualStep(this, 5)) {
        al::setSensorRadius(this, "Explosion", 0.0f);
        al::setSensorRadius(this, "ExplosionToPlayer", 0.0f);

        al::invalidateHitSensor(this, "Explosion");
        al::invalidateHitSensor(this, "ExplosionToPlayer");

        this->kill();
    }
}

namespace
{
NERVE_IMPL(TuccondorFur, Wait)
NERVE_IMPL(TuccondorFur, Wander)
NERVE_IMPL(TuccondorFur, Turn)
NERVE_IMPL(TuccondorFur, Find)
NERVE_IMPL(TuccondorFur, Chase)
NERVE_IMPL(TuccondorFur, Fall)
NERVE_IMPL(TuccondorFur, Land)
NERVE_IMPL(TuccondorFur, Attack)
NERVE_IMPL(TuccondorFur, CapHit)
NERVE_IMPL(TuccondorFur, BlowDown)
NERVE_IMPL(TuccondorFur, Explode)
} // namespace

} // namespace ca