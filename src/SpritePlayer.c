#include <stdbool.h>
#include "SpritePlayer.h"
#include "Keys.h"
#include "Scroll.h"
#include "Print.h"
#include "Sound.h"
#include "Banks/SetAutoBank.h"
#include "gb/gb.h"
#include "StateGame.h"

// player animations - the first number indicates the number of frames
const UINT8 anim_idle[] = {4, 0, 1, 2, 3};		
const UINT8 anim_idle_shoot[] = {1, 18};		
const UINT8 anim_walk[] = {6, 4, 5, 6, 7, 8, 9};
const UINT8 anim_walk_shoot[] = {1, 18};
const UINT8 anim_jump[] = {3, 10, 11, 12};
const UINT8 anim_fall[] = {3, 14, 15, 16};
const UINT8 anim_jump_shoot[] = {1, 13};
const UINT8 anim_attack[] = {4, 17, 18, 19, 20};
const UINT8 anim_climb[] = {4, 21, 22, 23, 24};
const UINT8 anim_climb_idle[] = {1, 25};
const UINT8 anim_hit[] = {6, 26, 27, 28, 26, 27, 28};
const UINT8 anim_die[] = {15, 26, 27, 28, 26, 27, 28, 29, 30, 31, 32, 32, 32, 32};
const UINT8 anim_appear[] = {3, 33, 34, 35};
const UINT8 anim_disappear[] = {5, 35, 34, 35, 34, 33};
const UINT8 anim_victory[] = {2, 34, 35}; // TBD


Sprite* player_sprite;
Sprite* attack1_sprite;
extern Sprite* attack_particle;
extern UINT8 start_x, start_y;
extern INT16 scroll_x, scroll_y;
static PlayerState curPlayerState, prevPlayerState;
static AnimationState lastAnimState, currentAnimState;

INT16 accel_y;
UINT8 decel_x;
UINT8 reset_x;
UINT8 reset_y;
UINT8 throw_cooldown;
UINT8 bg_hidden;
//UINT8 g_player_region;
UINT8 pause_secs;
UINT8 pause_ticks;
UINT8 invincible_secs;
UINT8 invincible_ticks;
UINT8 anim_hit_counter;
UINT8 player_spawned;

extern UINT16 timer_countdown;
extern UINT16 level_max_time;
extern UINT8 level_complete;
extern UINT16 level_width;
extern UINT16 level_height;

static void SetPlayerState(PlayerState state) {
	prevPlayerState = curPlayerState;
	curPlayerState = state;
}

static PlayerState GetPlayerState(void) {
	return curPlayerState;
}

static bool GetPlayerStateEquals(PlayerState ps) {
	return curPlayerState == ps;
}

static void SetAnimationState(AnimationState state) {
	lastAnimState = currentAnimState;
	currentAnimState = state;
	switch (state) {
		case WALK:    		SetSpriteAnim(THIS, anim_walk, WALK_ANIM_SPEED); break;
		case WALK_IDLE:    	SetSpriteAnim(THIS, anim_idle, DEFAULT_ANIM_SPEED);	break;
		case JUMP:    		SetSpriteAnim(THIS, anim_jump, DEFAULT_ANIM_SPEED); break;
		case FALL:    		SetSpriteAnim(THIS, anim_fall, DEFAULT_ANIM_SPEED); break;
		case ATTACK:		if (lastAnimState == JUMP) {
								SetSpriteAnim(THIS, anim_jump_shoot, DEFAULT_ANIM_SPEED);
							} else {
								SetSpriteAnim(THIS, anim_walk_shoot, DEFAULT_ANIM_SPEED);
							}
							//SetSpriteAnim(THIS, anim_attack, DEFAULT_ANIM_SPEED);
							break;
		case CLIMB:			SetSpriteAnim(THIS, anim_climb, DEFAULT_ANIM_SPEED); break;
		case CLIMB_IDLE:	SetSpriteAnim(THIS, anim_climb_idle, DEFAULT_ANIM_SPEED); break; 			
		case HIT:			SetSpriteAnim(THIS, anim_hit, HIT_ANIM_SPEED); break;
		case DIE:    		SetSpriteAnim(THIS, anim_die, DIE_ANIM_SPEED); break;
		case APPEAR:		SetSpriteAnim(THIS, anim_appear, DISAPPEAR_ANIM_SPEED); break;
		case DISAPPEAR:		SetSpriteAnim(THIS, anim_disappear, DISAPPEAR_ANIM_SPEED); break;
		case VICTORY: 		SetSpriteAnim(THIS, anim_victory, VICTORY_ANIM_SPEED); break;
	}
}

static AnimationState GetAnimationState(void) {
	return currentAnimState;
}

void StartLevel() {
	PlayerData* data = (PlayerData*)THIS->custom_data;
	// move player to start/checkpoint
	THIS->x = data->start_x;
	THIS->y = data->start_y;
	// reset time
	timer_countdown = level_max_time;
	data->timeup = 0;
	data->anim_playing = false;
	data->invincible = true;
	player_spawned = true;
	level_complete = false;
	ScrollRelocateMapTo(0, 0);
	SetPlayerState(PLAYER_STATE_APPEAR);
	SetAnimationState(APPEAR);
}

void UpdateAttackPos() {
	attack1_sprite->mirror = THIS->mirror;
	if(THIS->mirror == V_MIRROR) 
		attack1_sprite->x = THIS->x - 14u;
	else
		attack1_sprite->x = THIS->x + 14u; 
	attack1_sprite->y = THIS->y + 2u;
}

void Hit(Sprite* sprite, UINT8 idx) {
	PlayerData* data = (PlayerData*)THIS->custom_data;
	if (GetPlayerState() == PLAYER_STATE_HIT) return;
	if (data->anim_playing) return;
	if (data->invincible) return;
	if (--data->lives <= 0) {
		SetPlayerState(PLAYER_STATE_DIE);
		PlayFx(CHANNEL_1, 10, 0x5b, 0x7f, 0xf7, 0x15, 0x86);
		SetAnimationState(DIE);
		data->anim_playing = true;
		pause_secs = 6;
	} else {
		SetPlayerState(PLAYER_STATE_HIT);
		PlayFx(CHANNEL_1, 10, 0x5b, 0x7f, 0xf7, 0x15, 0x86);
		SetAnimationState(HIT);
		data->anim_playing = true;
	}
	Hud_Update();	
}

void Collected(Sprite* sprite, ItemType itype, UINT8 idx) {
	PlayerData* data = (PlayerData*)THIS->custom_data;
	switch (itype) {
		case ITEM_COIN:
			PlayFx(CHANNEL_1, 10, 0x5b, 0x7f, 0xf7, 0x15, 0x86);
			data->coins++;
			break;
		case ITEM_SPIRIT:
			data->spirits++;
			PlayFx(CHANNEL_1, 10, 0x5b, 0x7f, 0xf7, 0x15, 0x86);
			break;
		default:
			break;
	}
	Hud_Update();
}

void Attack() {
	SetPlayerState(PLAYER_STATE_ATTACKING);
	SetAnimationState(ATTACK);
	attack1_sprite = SpriteManagerAdd(SpriteAttack1, THIS->x, THIS->y);
	UpdateAttackPos();
	PlayFx(CHANNEL_4, 20, 0x0d, 0xff, 0x7d, 0xc0);
}

void Throw() {
	PlayerData* data = (PlayerData*)THIS->custom_data;
	// no knives left
	if (data->knives == 0) return;
	SetPlayerState(PLAYER_STATE_ATTACKING);
	SetAnimationState(ATTACK);
	Sprite* knife_sprite = SpriteManagerAdd(SpriteKnife, 0, 0);
	knife_sprite->mirror = THIS->mirror;
	if (THIS->mirror) {
		knife_sprite->x = THIS->x - 2u;
	} else {
		knife_sprite->x = THIS->x + 7u; 
	}	
	knife_sprite->y = THIS->y + 5u;
	data->knives--;
	// reset cooldown
	throw_cooldown = 10;
}

UINT8 tile_collision;
void CheckCollisionTile(Sprite* sprite, UINT8 idx) {
	if (tile_collision == TILE_INDEX_SPIKE_UP || tile_collision == TILE_INDEX_SPIKE_DOWN) {
		Hit(sprite, idx);
	} else if (tile_collision == TILE_INDEX_WATER_1 || tile_collision == TILE_INDEX_WATER_2) {
		Hit(sprite, idx);
	}
}

void HandleInput(Sprite* sprite, UINT8 idx) {
	if (GetPlayerState() == PLAYER_STATE_HIT || GetPlayerState() == PLAYER_STATE_DIE) return;
	/*
	if (decel_x > 0) {
		tile_collision = TranslateSprite(sprite, 1 << delta_time, 0);
		THIS->mirror = NO_MIRROR;
		CheckCollisionTile(sprite, idx);
		if (GetPlayerState() != PLAYER_STATE_JUMPING && GetPlayerState() != PLAYER_STATE_CLIMBING) {
			SetPlayerState(PLAYER_STATE_WALKING);
			SetAnimationState(WALK);
		}
		decel_x--;
	}
	if (decel_x < 0) {
		tile_collision = TranslateSprite(sprite, -1 << delta_time, 0);
		THIS->mirror = V_MIRROR;
		CheckCollisionTile(sprite, idx);
		if (GetPlayerState() != PLAYER_STATE_JUMPING && GetPlayerState() != PLAYER_STATE_CLIMBING) {
			SetPlayerState(PLAYER_STATE_WALKING);
			SetAnimationState(WALK);
		}	
		decel_x++;
	}*/
	if (KEY_PRESSED(J_RIGHT)) {
		if (GetPlayerState() == PLAYER_STATE_CLIMBING) {
			UINT8 tile = GetScrollTile((player_sprite->x + 16u) >> 3, (player_sprite->y + 16u) >> 3);
			SetAnimationState(WALK);
			if (tile != TILE_INDEX_LADDER_LEFT && tile != TILE_INDEX_LADDER_RIGHT) {
				SetPlayerState(PLAYER_STATE_WALKING);
				SetAnimationState(WALK);
				return;
			}
		}
		if (THIS->x < level_width-16) {
			tile_collision = TranslateSprite(sprite, 1 << delta_time, 0);
			CheckCollisionTile(sprite, idx);
		}
		THIS->mirror = NO_MIRROR;
		if (GetPlayerState() != PLAYER_STATE_JUMPING && GetPlayerState() != PLAYER_STATE_CLIMBING) {
			SetPlayerState(PLAYER_STATE_WALKING);
			SetAnimationState(WALK);
		}
		/*if (decel_x < MAX_DECEL_X) {
			decel_x++;
		}*/
	} else if (KEY_PRESSED(J_LEFT)) {
		if (GetPlayerState() == PLAYER_STATE_CLIMBING) {
			UINT8 tile = GetScrollTile((player_sprite->x) >> 3, (player_sprite->y + 16u) >> 3);
			SetAnimationState(WALK);
			if (tile != TILE_INDEX_LADDER_LEFT && tile != TILE_INDEX_LADDER_RIGHT) {
				SetPlayerState(PLAYER_STATE_WALKING);
				SetAnimationState(WALK);
				return;
			}
		}
		if (THIS->x > 16) { 
			tile_collision = TranslateSprite(sprite, -1 << delta_time, 0);
			CheckCollisionTile(sprite, idx);
		}
		THIS->mirror = V_MIRROR;
		if (GetPlayerState() != PLAYER_STATE_JUMPING && GetPlayerState() != PLAYER_STATE_CLIMBING) {
			SetPlayerState(PLAYER_STATE_WALKING);
			SetAnimationState(WALK);
		}
		/*if (decel_x > -(MAX_DECEL_X)) {
			decel_x--;
		}*/
	} else if (KEY_PRESSED(J_UP)) {
		UINT8 tile = GetScrollTile((player_sprite->x + 8u) >> 3, (player_sprite->y + 8u) >> 3);
		if (tile == TILE_INDEX_LADDER_LEFT || tile == TILE_INDEX_LADDER_RIGHT) {
			// move to center of ladder
			THIS->x = (((THIS->x)>> 3) << 3) + 4;
			accel_y = 0;
			tile_collision = TranslateSprite(THIS, 0, -1 << delta_time);
			CheckCollisionTile(sprite, idx);
			SetPlayerState(PLAYER_STATE_CLIMBING);
			SetAnimationState(CLIMB);
		}
	} else if (KEY_PRESSED(J_DOWN)) {
		UINT8 tile = GetScrollTile((player_sprite->x + 8u) >> 3, (player_sprite->y - 16u) >> 3);
		if (tile == TILE_INDEX_LADDER_LEFT || tile == TILE_INDEX_LADDER_RIGHT) {
			// move to center of ladder
			THIS->x = (((THIS->x)>> 3) << 3) + 4;
			accel_y = 0;
			tile_collision = TranslateSprite(THIS, 0, 1 << delta_time);
			CheckCollisionTile(sprite, idx);
			SetPlayerState(PLAYER_STATE_CLIMBING);
			SetAnimationState(CLIMB);
		}
	}
	if (KEY_TICKED(J_A) && (GetPlayerState() != PLAYER_STATE_JUMPING)) {
		PlayFx(CHANNEL_1, 5, 0x17, 0x9f, 0xf3, 0xc9, 0xc4);
		SetPlayerState(PLAYER_STATE_JUMPING);
		SetAnimationState(JUMP);
		accel_y = -50;
	} else {
		// check if now FALLING?
		if ((accel_y >> 4) > 1) {
			SetAnimationState(FALL);
		}
	}
	if (KEY_TICKED(J_B) && (GetPlayerState() != PLAYER_STATE_ATTACKING && GetPlayerState() != PLAYER_STATE_HIT && GetPlayerState() != PLAYER_STATE_DIE)) {
		if (KEY_PRESSED(J_UP)) {
			if (!throw_cooldown) {
				Throw();
			}
		} else {
			Attack();
		}
	}
	// nothing happening lets revert to idle state
	if (keys == 0 && !(GetPlayerState() == PLAYER_STATE_JUMPING || GetPlayerState() == PLAYER_STATE_ATTACKING)) {
		if (GetPlayerState() == PLAYER_STATE_CLIMBING) {
			SetAnimationState(CLIMB_IDLE);
		} else {
			SetAnimationState(WALK_IDLE);
		}
	}
	
	// decrement throw cooldown so we can throw again
	if (GetPlayerState() != PLAYER_STATE_HIT) {
		if (throw_cooldown) {
			throw_cooldown -= 1u;
		}
	}
}

//

void START() {
	player_sprite = THIS;
	PlayerData* data = (PlayerData*)THIS->custom_data;
	data->start_x = THIS->x;
	data->start_y = THIS->y;
	data->anim_playing = 0;
	data->lives = MAX_LIVES;
	data->knives = 10;
	data->coins = 0;
	data->spirits = 0;
	data->timeup = 0;
	data->invincible = 0;
	curPlayerState = PLAYER_STATE_IDLE;
	accel_y = 0;
	decel_x = 0;
	throw_cooldown = 0;
	bg_hidden = 0;
	scroll_target = THIS;
	reset_x = 20;
	reset_y = 80;
	attack1_sprite = 0;
	anim_hit_counter = 0;
	pause_secs = 0;
	pause_ticks = 0;
	StartLevel();
}

void UPDATE() {
	PlayerData* data = (PlayerData*)THIS->custom_data;
	UINT8 i;
	Sprite* spr;

	if (pause_secs) {
		pause_ticks++;
		if (pause_ticks == 25) {
			pause_ticks = 0;
			pause_secs--;
		}
		return;
	}

	if (invincible_secs) {
		invincible_ticks++;
		if (invincible_ticks == 25) {
			invincible_ticks = 0;
			invincible_secs--;
		}
	} else {
		data->invincible = false;
	}

	if (data->timeup) {
		data->lives--;
		Hud_Update();
		if (data->lives <= 0) { 
			SetState(StateGameOver);
			HIDE_WIN;
		}
		SetState(StateTimeUp);
		HIDE_WIN;
	}
	
	if (player_spawned) {
		if (THIS->anim_frame == 2) {
			SetPlayerState(PLAYER_STATE_IDLE);
			SetAnimationState(NORMAL);
			player_spawned = false;
		}
		return;
	}

	if (level_complete) {
		if (THIS->anim_frame == 2) {
			data->anim_playing = false;
			if (g_level_current == MAX_LEVEL) {
				SetState(StateWin);
				HIDE_WIN;
			} else {
				g_level_current++;
				SetState(StateGame);
			}
		}
		return;
	}

	switch (GetPlayerState()) {
		//case PLAYER_STATE_BEFORE_JUMP:
		//	if (THIS->anim_frame == 2) {
		//		SetPlayerState(PLAYER_STATE_JUMPING);
		//		SetAnimationState(JUMP);
		//		accel_y = -50;
		//	}
		//	break;
		case PLAYER_STATE_ATTACKING:
			UpdateAttackPos();
			if (THIS->anim_frame == 3) {
				//animation_playing = 0;
				data->anim_playing = 0;
				SetAnimationState(lastAnimState);
			}
			break;
		case PLAYER_STATE_JUMPING:
			// check if now FALLING?
			if ((accel_y >> 4) > 1) {
				SetPlayerState(PLAYER_STATE_FALLING);
				SetAnimationState(FALL);
			}
			break;
		case PLAYER_STATE_HIT:
			accel_y = 0;
			if (THIS->anim_frame == 5) {
				data->anim_playing = false;
				StartLevel();
			}
			return;
			break;
		case PLAYER_STATE_DIE:
			if (THIS->anim_frame == 14) {
				data->anim_playing = false;
				SetState(StateGameOver);
				HIDE_WIN;
			}
			return;
			break;
		case PLAYER_STATE_CLIMBING: 	
			UINT8 tile = GetScrollTile((player_sprite->x + 8u) >> 3, (player_sprite->y + 22u) >> 3);
			if (tile != TILE_INDEX_LADDER_LEFT && tile != TILE_INDEX_LADDER_RIGHT) {
				//SetPlayerState(PLAYER_STATE_IDLE);
			}
			break;
		default:
			if (keys == 0 && !data->anim_playing) {
				if (GetPlayerState() == PLAYER_STATE_CLIMBING) {
					SetAnimationState(CLIMB_IDLE);
				} else {
					SetAnimationState(WALK_IDLE);
				}
			}
			player_spawned = false;

	}

	//if (currentAnimState == ATTACK && shoot_cooldown == 0) {
	//	SetAnimationState(lastAnimState);
	//}

	HandleInput(THIS, THIS_IDX);

	// simple gravity physics until we "collide" with something and not on a ladder
	if (curPlayerState != PLAYER_STATE_CLIMBING) {
		if (accel_y < 40) {
			accel_y += 2;
		}
		//if (THIS->y < 16) return;
		tile_collision = TranslateSprite(THIS, 0, accel_y >> 4);
		if (!tile_collision && delta_time != 0 && accel_y < 40) { 
			//do another iteration if there is no collision
			accel_y += 2;
			tile_collision = TranslateSprite(THIS, 0, accel_y >> 4);
		}
		if (tile_collision) {
			accel_y = 0;
			if (currentAnimState == JUMP || currentAnimState == FALL) {
				currentAnimState = WALK_IDLE;
			}
			curPlayerState = PLAYER_STATE_IDLE;
			CheckCollisionTile(THIS, THIS_IDX);
		}
	} else {
		// TBD
	}	

	// check enemy sprite collision - item colission is in each item sprite
	for (i = 0u; i != sprite_manager_updatables[0]; ++i) {
		spr = sprite_manager_sprites[sprite_manager_updatables[i + 1u]];
		if (spr->type == SpriteEnemy1 || spr->type == SpriteBat) {
			if (CheckCollision(THIS, spr) && !data->invincible) {
				Hit(THIS, THIS_IDX);
			}
		} 
		if (spr->type == SpritePortal) {
			if (CheckCollision(THIS, spr) && !player_spawned && THIS->x > 16) {
				THIS->x = spr->x-8;
				SetAnimationState(DISAPPEAR);
				SetPlayerState(PLAYER_STATE_DISAPPEAR);
				data->anim_playing = true;
				level_complete = true;
				//pause_secs = 5;
			}
		}
	}

}

void DESTROY() {
}




