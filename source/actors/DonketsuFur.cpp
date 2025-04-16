#include "actors/DonketsuFur.hpp"
#include "rs/util.hpp"
#include "al/util.hpp"
#include "game/Player/PlayerHitPointData.h"

namespace ca {

// DonketsuFur *donketsuInstance;

DonketsuFur::DonketsuFur(const char *name) : al::LiveActor(name) { }

typedef void (DonketsuFur::*functorType)();

void DonketsuFur::init(al::ActorInitInfo const &info)
{
    al::initActorWithArchiveName(this, info, "DonketsuFur", nullptr);

    // if (al::isValidStageSwitch(this, "SwitchStart")) {
    //     gLogger->LOG("Valid.\n");
    //     al::initNerve(this, &nrvDonketsuFurWait, 1);
    // } else {
    //     gLogger->LOG("Invalid.\n");
    al::initNerve(this, &nrvDonketsuFurWander, 1);
    // }    

    this->state = new EnemyStateWander(this, "Walk");

    al::initNerveState(this, this->state, &nrvDonketsuFurWander, "徘徊");

    this->forceKeeper = new ExternalForceKeeper();

    //gLogger->LOG("Registering Listen Appear functor.\n");

    // if (al::listenStageSwitchOnAppear(
    //         this, al::FunctorV0M<CustomTogezo*, functorType>(this, &CustomTogezo::listenAppear))) {
    //     gLogger->LOG("Switch On Activated. Making Actor Dead.\n");   
    //     this->makeActorDead();
    // } else {
        //gLogger->LOG("Switch On not Active. Making Actor Alive.\n");   
        this->makeActorAlive();
    // }

    al::setSensorRadius(this, "Explosion", 0.0f);
    al::setSensorRadius(this, "ExplosionToPlayer", 0.0f);

    al::invalidateHitSensor(this, "Explosion");
    al::invalidateHitSensor(this, "ExplosionToPlayer");

    ca::donketsuInstance = this; 
}

void DonketsuFur::listenAppear() {
    this->appear();
}

bool DonketsuFur::receiveMsg(const al::SensorMsg* message, al::HitSensor* source,
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
        return true;
    }

    if(rs::isMsgCapReflect(message) && !al::isNerve(this, &nrvDonketsuFurBlowDown) && this->capHitCooldown <= 0) {
        rs::requestHitReactionToAttacker(message, target, source);
        al::setNerve(this, &nrvDonketsuFurCapHit);
        this->capPos = *al::getSensorPos(source);
        this->capHitCooldown = 10;
        return true;
    }

    if((rs::isMsgBlowDown(message) || rs::isMsgDonsukeAttack(message)) && !al::isNerve(this, &nrvDonketsuFurBlowDown)) {
        al::setVelocityBlowAttackAndTurnToTarget(this, *al::getActorTrans(source), 15.0f, 35.0f);
        rs::setAppearItemFactorAndOffsetByMsg(this, message, source);
        rs::requestHitReactionToAttacker(message, target, source);
        al::setNerve(this, &nrvDonketsuFurBlowDown);
        return true;
    }

    if(!rs::isMsgPechoSpot(message) && !rs::isMsgDamageBallAttack(message) && !al::isMsgPlayerFireBallAttack(message) || !al::isSensorEnemyBody(target)) {
        return this->forceKeeper->receiveMsg(message, source, target);
    }

    rs::setAppearItemFactorAndOffsetByMsg(this, message, source);
    rs::requestHitReactionToAttacker(message, target, source);
    al::startHitReaction(this, "死亡");
    this->kill();
    return false;
}

void DonketsuFur::attackSensor(al::HitSensor* source, al::HitSensor* target) {

    if(al::isNerve(this, &nrvDonketsuFurExplode)) {
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
    else if (!al::isNerve(this, &nrvDonketsuFurBlowDown)) {
        if (al::isSensorPlayer(source)) {
            rs::sendMsgPushToPlayer(target, source);
            return;
        }
        else if (!al::sendMsgEnemyAttack(target, source)) {
            rs::sendMsgPushToPlayer(target, source);
            if (al::isNerve(this, &nrvDonketsuFurBlowDown))
                return;
            
            if (al::isNerve(this, &nrvDonketsuFurBlowDown) || al::isNerve(this, &nrvDonketsuFurAttack) ||
                 !al::sendMsgEnemyAttack(target, source)) {
                al::sendMsgPushAndKillVelocityToTarget(this, source, target);
                return;
            }
        }
        al::setNerve(this, &nrvDonketsuFurAttack);
    }
}


// todo: no idea what 0x144 or 0x124 are

void DonketsuFur::control() {
    
    if (al::isInDeathArea(this) || al::isCollidedFloorCode(this, "DamageFire") ||
        al::isCollidedFloorCode(this, "Needle") || al::isCollidedFloorCode(this, "Poison") ||
        al::isInWaterArea(this)) {
        al::tryEmitEffect(this, "Disappear", nullptr);
        al::setEffectAllScale(this, "Disappear", sead::Vector3f(2.0f,2.0f,2.0f));
        this->kill();
    } else {
        // this is probably wrong but matches
        int unk = this->capHitCooldown - 1;
        if(unk >= 0) {
            this->capHitCooldown = unk;
        }

        sead::Vector3f calculatedForce = sead::Vector3f::zero;

        this->forceKeeper->calcForce(&calculatedForce);
        
        this->futurePos = ((calculatedForce * 0.64) + this->futurePos) * 0.955;

        this->forceKeeper->reset();

        if(al::isNearZero(calculatedForce, 0.001)) {
            this->unkInt = 180;
            al::invalidateClipping(this);
        }

        int prevInt = this->unkInt;
        if(prevInt-- > 0) {
            this->unkInt = prevInt;

            if (prevInt == 0) {
                if(al::isNerve(this, &nrvDonketsuFurWander)) {
                    al::validateClipping(this);
                }
            }
        }

        if(this->explodeTimer > 0) {
            this->explodeTimer--;
            if(this->explodeTimer == 0) {
                al::setNerve(this, &nrvDonketsuFurExplode);
            }
        }
    }
}

void DonketsuFur::updateCollider() {
    sead::Vector3f& velocity = al::getVelocity(this);

    if (al::isNoCollide(this)) {
        *al::getTransPtr(this) += velocity;
        al::getActorCollider(this)->onInvalidate();
    } else {
        if (al::isFallOrDamageCodeNextMove(this, (velocity + this->futurePos) * 1.5, 50.0f, 200.0f)) {
            *al::getTransPtr(this) += al::getActorCollider(this)->collide((velocity + this->futurePos) * 1.5f);
        }else {
            sead::Vector3f result = al::getActorCollider(this)->collide(velocity + this->futurePos);
            *al::getTransPtr(this) += result;
        }
    }
}

void DonketsuFur::updateVelocity() {
    if(al::isOnGround(this, 0)) {
        sead::Vector3f *groundNormal = al::getOnGroundNormal(this, 0);
        al::getVelocity(this);
        if(al::isFallOrDamageCodeNextMove(this, al::getVelocity(this), 50.0f, 200.0f)) {
            float velY = al::getVelocity(this).y;
            al::scaleVelocity(this, -1.0f);
            al::getVelocityPtr(this)->y = velY;
        }else {
            al::addVelocity(this, sead::Vector3f(-groundNormal->x, -groundNormal->y,-groundNormal->z));
            al::scaleVelocity(this, 0.95f);
        }
    }else {
        al::addVelocityY(this, -2.0f);
        al::scaleVelocity(this, 0.98f);
    }
}

void DonketsuFur::exeWait(void)  // 0x0
{
    if (al::isFirstStep(this)) {
        al::startAction(this, "Wait");
        al::setVelocityZero(this);
    }
    if (al::isValidStageSwitch(this, "SwitchStart") && al::isOnStageSwitch(this, "SwitchStart")) {
        al::setNerve(this, &nrvDonketsuFurWander);
    }
}

void DonketsuFur::exeWander(void)  // 0x8
{
    if (al::isFirstStep(this)) {
        al::setVelocityZero(this);
        al::startAction(this, "Walk");
    }

    al::updateNerveState(this);

    bool isGrounded = al::isOnGround(this, 0);
    bool isNearPlayer = al::isNearPlayer(this, 1000.0f);

    if (isGrounded && isNearPlayer) {
        al::setNerve(this, &nrvDonketsuFurTurn);
    } else if (isGrounded) {
        this->airTime = 0;
        this->groundNormal = *al::getOnGroundNormal(this, 0);
    } else {
        this->airTime++;

        if (this->airTime > 4) {
            al::setNerve(this, &nrvDonketsuFurFall);
        }
    }
}

// FIXME vector math is non-matching, but seems to be functionally identical.
void DonketsuFur::exeCapHit(void)  // 0x10
{
    sead::Quatf frontUp;

    if (al::isFirstStep(this)) {
        al::startAction(this, "CapHit");
        sead::Vector3f& actorPos = al::getTrans(this);

        sead::Vector3f capDirection =
            sead::Vector3f(actorPos.x - this->capPos.x, 0.0f, actorPos.z - this->capPos.z);

        al::tryNormalizeOrDirZ(&capDirection, capDirection);

        al::setVelocity(this, capDirection * 20.0f);

        al::makeQuatUpFront(&frontUp, capDirection, sead::Vector3f::ey);

        this->airTime = 0;

        al::invalidateClipping(this);
    }

    if (al::isActionEnd(this)) {
        if (al::isNearPlayer(this, 1000.0f)) {
            al::setNerve(this, &nrvDonketsuFurFind);
        } else {
            al::setNerve(this, &nrvDonketsuFurWander);
        }
    } else if (al::isOnGround(this, 0)) {
        this->airTime = 0;

        al::addVelocityToGravity(this, 1.0);

        al::scaleVelocity(this, 0.95f);

        sead::Vector3f& velocity = al::getVelocity(this);

        sead::Vector3f unk = sead::Vector3f(velocity.x, 0.0f, velocity.z);

        if (al::tryNormalizeOrZero(&unk, unk)) {
            sead::Vector3f unk2 = unk * 10.0f;

            if (al::isFallOrDamageCodeNextMove(this, unk2, 50.0f, 200.0f)) {
                al::setVelocity(this, unk2 * 5.0f);
            }
        }

    } else {
        this->airTime++;

        if (this->airTime > 5) {
            al::setNerve(this, &nrvDonketsuFurFall);
        } else {
            al::addVelocityToGravity(this, 1.0);
            al::scaleVelocity(this, 0.98f);
        }
    }
}

void DonketsuFur::exeAttack(void)  // 0x20
{
    if (al::isFirstStep(this)) {
        al::startAction(this, "AttackSuccess");
        al::setVelocityZero(this);
    }

    this->updateVelocity();

    if (al::isActionEnd(this)) {
        al::setNerve(this, &nrvDonketsuFurWander);
    }
}

void DonketsuFur::exeTurn(void)  // 0x28
{
    if(al::isFirstStep(this)) {
        al::setVelocityZero(this);
        al::startAction(this, "Turn");
    }
    sead::Vector3f frontDir = sead::Vector3f::zero;
    al::calcFrontDir(&frontDir, this);

    PlayerActorHakoniwa* pActor = al::tryFindNearestPlayerActor(this);
    if(pActor) {
        if(al::isFaceToTargetDegreeH(this, al::getTrans(pActor), frontDir, 1.0f)) {
            al::setNerve(this, &nrvDonketsuFurFind);
            return;
        }
        al::turnToTarget(this, al::getTrans(pActor), 3.5f);
    }
    if(!al::isNearPlayer(this, 1300.0f)) {
        al::setNerve(this, &nrvDonketsuFurWander);
        return;
    }
    if(al::isOnGround(this, 0)) {
        this->airTime = 0;
        return;
    }
    al::addVelocityToGravity(this, 1.0f);
    al::scaleVelocity(this, 0.98f);
    if(this->airTime++ >= 4) {
        al::setNerve(this, &nrvDonketsuFurFall);
    }
}

void DonketsuFur::exeFall(void)  // 0x30
{
    if (al::isFirstStep(this)) {
        al::invalidateClipping(this);
        al::startAction(this, "Fall");
    }

    this->updateVelocity();

    if (al::isOnGround(this, 0)) {
        this->airTime = 0;
        al::validateClipping(this);
        al::setNerve(this, &nrvDonketsuFurLand);
    }
}

void DonketsuFur::exeFind(void)  // 0x38
{
    if (al::isFirstStep(this)) {
        al::setVelocityZero(this);
        al::startAction(this, "Find");
        this->airTime = 0;
        al::invalidateClipping(this);
    }
    if (!al::isOnGround(this, 0) && this->airTime++ >= 4) {
        al::setNerve(this, &nrvDonketsuFurFall);
    } else {
        this->updateVelocity();
        if (!al::isActionEnd(this))
            return;
        al::setNerve(this, &nrvDonketsuFurChase);
    }
}

void DonketsuFur::exeChase(void)  // 0x40
{
    if (al::isFirstStep(this)) {
        al::startAction(this, "Run");
        al::invalidateClipping(this);
    }
    if(al::isOnGround(this, 0)) {
        sead::Vector3f* groundNormal = al::getOnGroundNormal(this, 0);
        sead::Vector3f normalXZ;
        normalXZ.z = groundNormal->z;
        normalXZ.x = groundNormal->x;
        al::scaleVelocityDirection(this, normalXZ, 0.0f);
        this->airTime = 0;

        PlayerActorHakoniwa* pActor = al::tryFindNearestPlayerActor(this);
        if(pActor) {
            al::turnToTarget(this, al::getTrans(pActor), 3.5f);

            sead::Vector3f frontDir = sead::Vector3f::zero;
            al::calcFrontDir(&frontDir, this);
            sead::Vector3f vertical = sead::Vector3f::zero;
            sead::Vector3f horizontal = sead::Vector3f::zero;
            al::separateVectorHV(&horizontal, &vertical, normalXZ, frontDir);
            al::tryNormalizeOrDirZ(&horizontal, horizontal);
            horizontal *= 0.8;
            al::addVelocity(this, horizontal); // not matching, however doing it the matching way causes issues for some reason
            al::scaleVelocity(this, 0.95);
        }
        if(!al::isNearPlayer(this, 1300.0f)) {
            al::setNerve(this, &nrvDonketsuFurWander);
            return;
        }else if(al::isNearPlayer(this, 700.0f) && this->explodeTimer == 0) {

                this->explodeTimer = -1;
        }
    }else {
        if (this->airTime++ >= 4) {
            al::setNerve(this, &nrvDonketsuFurFall);
            return;
        }
    }

    this->updateVelocity();
    
}

void DonketsuFur::exeLand(void)  // 0x48
{
    int* airTimePtr;

    if (al::isFirstStep(this)) {
        al::setVelocityZero(this);
        al::startAction(this, "Land");
        airTimePtr = &this->airTime;
        this->airTime = 0;
    } else {
        airTimePtr = &this->airTime;
    }

    this->updateVelocity();

    if (!al::isOnGround(this, 0) && (*airTimePtr)++ >= 4) {
        al::setNerve(this, &nrvDonketsuFurFall);
    } else {
        if (!al::isActionEnd(this))
            return;
        al::setNerve(this, &nrvDonketsuFurWander);
    }
}

void DonketsuFur::exeExplode(void) {
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
NERVE_IMPL(DonketsuFur, Wait)
NERVE_IMPL(DonketsuFur, Wander)
NERVE_IMPL(DonketsuFur, Turn)
NERVE_IMPL(DonketsuFur, Find)
NERVE_IMPL(DonketsuFur, Chase)
NERVE_IMPL(DonketsuFur, Fall)
NERVE_IMPL(DonketsuFur, Land)
NERVE_IMPL(DonketsuFur, Attack)
NERVE_IMPL(DonketsuFur, CapHit)
NERVE_IMPL(DonketsuFur, BlowDown)
NERVE_IMPL(DonketsuFur, Explode)
} // namespace

} // namespace ca