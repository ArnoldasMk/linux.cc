#include "resolver.h"

#include "global.h"
#include "../ImGUI/imgui.h"
#include "resolverAP.h"
std::vector<int64_t> Resolver::Players = {};
bool hasDesyncHadTimeToDesync = false;
bool hasDesyncHadTimeToLBY = false;
bool first = true;
#define RANDOME_FLOAT(x) ( static_cast<float>(static_cast<float>(rand()/ static_cast<float>(RAND_MAX/ ( x ) ))) )
#define GetPercentVal(val, percent) (val * (percent/100.f))
    float normalize_pitch(float pitch)
        {
                while (pitch > 89.0f)
                        pitch -= 180.0f;

                while (pitch < -89.0f)
                        pitch += 180.0f;

                return pitch;
        }

    Vector CalculateAngle(Vector src, Vector dst)
        {
        Vector angles;

        Vector delta = src - dst;
        float hyp = delta.Length2D();

        angles.y = std::atanh(delta.y / delta.x) * 57.2957795131f;
        angles.x = std::atanh(-delta.z / hyp) * -57.2957795131f;
        angles.z = 0.0f;

        if (delta.x >= 0.0f)
                angles.y += 180.0f;

        return angles;
        }



static float NormalizeAsYaw(float flAngle)
{
	if (flAngle > 180.f || flAngle < -180.f)
	{
		if (flAngle < 0.f)
			flAngle += round(abs(flAngle));
		else
			flAngle -= round(abs(flAngle));
	}
	return flAngle;
}
			
float GetBackwardYaw(C_BasePlayer* player) {
                C_BasePlayer *local = (C_BasePlayer *)entityList->GetClientEntity(engine->GetLocalPlayer());
        return CalculateAngle(local->GetAbsOrigin(), player->GetAbsOrigin()).y;
}

float GetAngle(C_BasePlayer* player) {
        C_BasePlayer *local = (C_BasePlayer *)entityList->GetClientEntity(engine->GetLocalPlayer());
        return NormalizeAsYaw(player->GetEyeAngles()->y);
}

float GetForwardYaw(C_BasePlayer* player) {
        C_BasePlayer *local = (C_BasePlayer *)entityList->GetClientEntity(engine->GetLocalPlayer());
        return NormalizeAsYaw(GetBackwardYaw(player) - 180.f);
}

float GetLeftYaw(C_BasePlayer* player) {
        C_BasePlayer *local = (C_BasePlayer *)entityList->GetClientEntity(engine->GetLocalPlayer());
        return NormalizeAsYaw(CalculateAngle(local->GetAbsOrigin(), player->GetAbsOrigin()).y - 90.f);
}

float GetRightYaw(C_BasePlayer* player) {
        C_BasePlayer *local = (C_BasePlayer *)entityList->GetClientEntity(engine->GetLocalPlayer());
        return NormalizeAsYaw(CalculateAngle(local->GetAbsOrigin(), player->GetAbsOrigin()).y + 90.f);
}

void DetectSide(C_BasePlayer* player, int *side)
{
        Vector src3D, dst3D, forward, right, up, src, dst;
        float back_two, right_two, left_two;
        trace_t tr;
        Ray_t ray, ray2, ray3, ray4, ray5;
        CTraceFilter filter;

        Math::AngleVectors(Vector(0, GetBackwardYaw(player), 0), &forward, &right, &up);

        filter.pSkip = player;
        src3D = player->GetEyePosition();
        dst3D = src3D + (forward * 384); //Might want to experiment with other numbers, incase you don't know what the number does, its how far the trace will go. Lower = shorter.

        ray.Init(src3D, dst3D);
        trace->TraceRay(ray, MASK_SHOT, &filter, &tr);
        back_two = (tr.endpos - tr.startpos).Length();

        ray2.Init(src3D + right * 35, dst3D + right * 35);
        trace->TraceRay(ray2, MASK_SHOT, &filter, &tr);
        right_two = (tr.endpos - tr.startpos).Length();

        ray3.Init(src3D - right * 35, dst3D - right * 35);
        trace->TraceRay(ray3, MASK_SHOT, &filter, &tr);
        left_two = (tr.endpos - tr.startpos).Length();

        if (left_two > right_two) {
                *side = -1;
                //Body should be right
        }
        else if (right_two > left_two) {
                *side = 1;
        }
        else
                *side = 0;
}
	bool IsDesyncing(C_BasePlayer* player)
	{
        model_t *pModel = player->GetModel();
        if( !pModel )
                return false;

        studiohdr_t *hdr = modelInfo->GetStudioModel(pModel);
        if( !hdr )
                return false;

		for (int k = 0; k < 13; k++)
		{
			auto overlay = player->GetAnimOverlay();
			if (!overlay)
				return false;
			const auto activity = player->GetSequenceActivity(overlay->operator[](k).m_nSequence);
			if (activity == 979)
				return true;
		}

		return false;
	}

bool CheckDesync (CCSGOAnimState* Animstate, C_BasePlayer* player){
 float Velocity = player->GetVelocity().Length2D();
    bool PresumedNoDesync = false;
    if ((Velocity < 140) && (Animstate->timeSinceStartedMoving <= 1.2f)) {
        if (Animstate->timeSinceStartedMoving <= 1.2f) {
            if (Animstate->timeSinceStoppedMoving >= .22f) {
                hasDesyncHadTimeToLBY = true;
                if (Animstate->timeSinceStoppedMoving >= .8f) {
                    hasDesyncHadTimeToDesync = true;
                }
            }
        } else if (Animstate->timeSinceStartedMoving > 0) {
            if (Velocity > 100) {
                hasDesyncHadTimeToDesync = true;
            }
        }
    }
    else{

        PresumedNoDesync = true;
    }
	return PresumedNoDesync;
    }

bool PitchCheck(C_BasePlayer* player)
{
        CUtlVector<AnimationLayer> *layers = player->GetAnimOverlay();
        const bool m_on_shot = player->GetSequenceActivity(layers->operator[](1).m_nSequence) == (int)CCSGOAnimStatePoses::ACT_CSGO_FIRE_PRIMARY;
        static float timer = globalVars->curtime + 250.f / 100.f;
        if (timer <= globalVars->curtime)
        {
			if (!m_on_shot && !player->GetDormant() && !(*csGameRules)->IsFreezeTime() && !player->GetFlags() & FL_FROZEN)
                        Resolver::players[player->GetIndex()].oldpitch = player->GetEyeAngles()->x;

                timer = globalVars->curtime + 250.f / 100.f;
        }

        if (fabsf(Resolver::players[player->GetIndex()].oldpitch - player->GetEyeAngles()->x) > 10.f)
	//if (Resolver::players[player->GetIndex()].oldpitch != player->GetEyeAngles()->x)
                return true;

        return false;
}

void Resolver::FrameStageNotify(ClientFrameStage_t stage)
{
	if (!engine->IsInGame())
		return;

	if (!Settings::Resolver::resolveAll)
		return;
		
	C_BasePlayer *localplayer = (C_BasePlayer *)entityList->GetClientEntity(engine->GetLocalPlayer());
	if (!localplayer || !localplayer->GetAlive())
		return;

if (Settings::Resolver::resolveAllAP){
ResolverAP::FrameStageNotify(stage);
return;
}

	if (stage == ClientFrameStage_t::FRAME_NET_UPDATE_POSTDATAUPDATE_START)
	{

		bool Pitch = Settings::Resolver::rPitch;
		int maxClient = engine->GetMaxClients();
		for (int i = 1; i < maxClient; ++i)
		{
			//indx = i;
			C_BasePlayer *player = (C_BasePlayer *)entityList->GetClientEntity(i);

			if (!player 
			|| player == localplayer 
			|| player->GetDormant() 
			|| !player->GetAlive() 
			|| player->GetImmune()
			|| Entity::IsTeamMate(player, localplayer))
				continue;

			IEngineClient::player_info_t entityInformation;
			engine->GetPlayerInfo(i, &entityInformation);
		if ( Resolver::players[player->GetIndex()].enemy )
			{
				if (player != Resolver::players[player->GetIndex()].enemy) // It means player discoennected or player sequence changed better to reset out miss shots count
				{
					Resolver::players[player->GetIndex()].MissedCount = 0;
					Resolver::players[player->GetIndex()].enemy = player;
				}
			}
			else 
			{
				Resolver::players[player->GetIndex()].enemy = player;
			}
			if (!Settings::Resolver::manual){
			Resolver::players[player->GetIndex()].fakeng = player->GetEyeAngles()->y;
        	        int wap;
                	int previous_ticks[128];
                	auto ticks = TIME_TO_TICKS(player->GetSimulationTime() - player->GetOldSimulationTime());
	                auto valid_lby = true;
			//bool fd = FakeDuckCheck(player);

	                if (ticks == 0 && previous_ticks[player->GetIndex()] > 0) {
                	wap = previous_ticks[player->GetIndex()] - 1;
                	}
                	else {
                	previous_ticks[player->GetIndex()] = ticks;
                	wap = ticks;
                	}
                	//if (player->GetAnimState()->currentFeetYaw != player->GetAnimState()->goalFeetYaw)
                	//Resolver::players[player->GetIndex()].choke = true;
                //	else
                	Resolver::players[player->GetIndex()].choke = IsDesyncing(player);
			if (first){
			player->SetupBones( Resolver::players[player->GetIndex()].ogmatrix, 128, BONE_USED_BY_HITBOX, globalVars->curtime );
			first = false;
			}
			if (!Resolver::players[player->GetIndex()].choke)
                                Resolver::players[player->GetIndex()].flags = rflag::NONE;


			if ((player->GetEyeAngles()->x < 65.f || player->GetEyeAngles()->x > 89.f) && PitchCheck(player) || Settings::Resolver::resolverType == resolverType::Legit)
			{
				if (Resolver::players[player->GetIndex()].choke){
				float trueDelta = NormalizeAsYaw(*player->GetLowerBodyYawTarget() - player->GetEyeAngles()->y);
		                Resolver::players[player->GetIndex()].flags = rflag::LAA;
	                if (player->GetVelocity().Length() > 0.1f || fabs(player->GetAnimState()->verticalVelocity) > 100.f)
	                         valid_lby = player->GetAnimState()->timeSinceStartedMoving < 0.22f;
				switch(Resolver::players[player->GetIndex()].MissedCount)
				{
					case 0:
						if (valid_lby)
						player->GetAnimState()->goalFeetYaw += 115;
						else
						player->GetEyeAngles()->y += AntiAim::GetMaxDelta(player->GetAnimState());
						break;
					case 1:
						if (valid_lby)
                                               	player->GetAnimState()->goalFeetYaw -= 115;
						else
                                                player->GetEyeAngles()->y -= AntiAim::GetMaxDelta(player->GetAnimState());
						break;
					case 2:
						player->GetEyeAngles()->y = trueDelta == 0 ? player->GetEyeAngles()->y - 30.f :  player->GetEyeAngles()->y + trueDelta;
						break;
					case 3:
						player->GetEyeAngles()->y += trueDelta;
						break;
					case 4:
						player->GetEyeAngles()->y = trueDelta == 0 ? player->GetEyeAngles()->y - 37.f : player->GetEyeAngles()->y + trueDelta;
						break;
					default:
						break;
				}
										}
			}
			else if (Resolver::players[player->GetIndex()].choke || Settings::Resolver::resolverType == resolverType::Rage)
            {
       CUtlVector<AnimationLayer> *layers = player->GetAnimOverlay();
        const bool m_on_shot = player->GetSequenceActivity(layers->operator[](1).m_nSequence) == (int)CCSGOAnimStatePoses::ACT_CSGO_FIRE_PRIMARY;
		auto animstate = player->GetAnimState();
                float trueDelta = NormalizeAsYaw(*player->GetLowerBodyYawTarget() - player->GetEyeAngles()->y);
		float angle = GetAngle(player);
		float fuck = 90;
                bool forward = fabsf(NormalizeAsYaw(GetAngle(player) - GetForwardYaw(player))) < 90.f;
		player->GetEyeAngles()->x = normalize_pitch(player->GetEyeAngles()->x);
                float delta = NormalizeAsYaw(player->GetEyeAngles()->y - player->GetAnimState()->goalFeetYaw);
		float initialyaw = 0.f;
		bool desynctime = CheckDesync(animstate , player);
	        if (player->GetVelocity().Length() > 0.1f || fabs(animstate->verticalVelocity) > 100.f)
       		         valid_lby = animstate->timeSinceStartedMoving < 0.22f;
		if (localplayer->GetAlive())
		Resolver::players[player->GetIndex()].delta = delta;
		else
                Resolver::players[player->GetIndex()].delta = 0;
		if (hasDesyncHadTimeToDesync || hasDesyncHadTimeToLBY){
	        if (fabs(delta) > 30.0f && valid_lby)
	        {
				Resolver::players[player->GetIndex()].flags = rflag::LBY;
                if (delta > 30.0f)
                {
			initialyaw = NormalizeAsYaw(player->GetEyeAngles()->y + 60.0f);
                }
                else if (delta < -30.0f)
                {
			initialyaw = NormalizeAsYaw(player->GetEyeAngles()->y - 60.0f);
                }
   		}
		else{
		if (delta > 0.0f)
		initialyaw = NormalizeAsYaw(player->GetEyeAngles()->y + 30.0f);
		else
                initialyaw = NormalizeAsYaw(player->GetEyeAngles()->y - 30.0f);

                                Resolver::players[player->GetIndex()].flags = rflag::LOW;

		}
		}else if (desynctime && !(hasDesyncHadTimeToDesync || hasDesyncHadTimeToLBY) || m_on_shot){
		 initialyaw = 0;
		Resolver::players[player->GetIndex()].flags = rflag::NOTIME;
		}
				switch(Resolver::players[player->GetIndex()].MissedCount)
				{
					case 0:
  						player->GetAnimState()->goalFeetYaw = NormalizeAsYaw(initialyaw);
						break;
					case 1:
                                                player->GetAnimState()->goalFeetYaw = NormalizeAsYaw(-initialyaw);
						break;
					case 2:
                                                ResolverAP::FrameStageNotify(stage);
                                                Resolver::players[player->GetIndex()].flags = rflag::AP;
						break;
					case 3:
						player->GetEyeAngles()->y = trueDelta <= 0 ? player->GetEyeAngles( )->y - 30.f : player->GetEyeAngles( )->y + 30.f;
                                                Resolver::players[player->GetIndex()].flags = rflag::BRUTE;
						break;
					case 4:
						player->GetEyeAngles()->y += trueDelta <= 0 ? player->GetEyeAngles( )->y - RANDOME_FLOAT(35.f) : player->GetEyeAngles( )->y + RANDOME_FLOAT(35.f);
                                                Resolver::players[player->GetIndex()].flags = rflag::BRUTE;
						break;
					default:
						break;
				}

            		}
			}
			else {
			if ( !Settings::Resolver::forcebrute){
			player->GetAnimState()->goalFeetYaw += Settings::Resolver::goalFeetYaw;
			player->GetEyeAngles()->y += Settings::Resolver::EyeAngles;
			 player->GetEyeAngles()->x = Settings::Resolver::Pitch;
			}else if ( Settings::Resolver::forcebrute){
			float trueDelta = NormalizeAsYaw(*player->GetLowerBodyYawTarget() - player->GetEyeAngles()->y);
 			float maxDesync = AntiAim::GetMaxDelta(player->GetAnimState());
 			int mD = (int) maxDesync;
                        float randNum = rand()%(mD-(-mD) + 1) + -mD;
			player->GetEyeAngles( )->y += randNum;
			}
			}
		}
	first = true;
	}
	else if (stage == ClientFrameStage_t::FRAME_RENDER_END)
	{
	}
}

void Resolver::FireGameEvent(IGameEvent *event)
{
	// if (!event)
	// 	return;

	// if (strcmp(event->GetName(), XORSTR("player_connect_full")) == 0 || strcmp(event->GetName(), XORSTR("cs_game_disconnected")) == 0)
    // {
	// 	if (event->GetInt(XORSTR("userid")) && engine->GetPlayerForUserID(event->GetInt(XORSTR("userid"))) != engine->GetLocalPlayer())
	//     	return;
    // }

	// 	int attacker_id = engine->GetPlayerForUserID(event->GetInt(XORSTR("attacker")));
	// 	int deadPlayer_id = engine->GetPlayerForUserID(event->GetInt(XORSTR("userid")));

	// 	if (attacker_id == deadPlayer_id) // suicide
	//     	return;
	// 	if (attacker_id != engine->GetLocalPlayer())
	// 		return;

	// 	if (strcmp(event->GetName(), "player_hurt") == 0 || strcmp(event->GetName(), "player_hurt") == -1);
			// Resolver::players[TargetID].MissedCount--;
				// ImGui::TextWrapped(XORSTR("Missed"));
}
