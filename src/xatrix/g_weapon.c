/* =======================================================================
 *
 * Weapon support functions.
 *
 * =======================================================================
 */

#include "header/local.h"

extern void SP_item_foodcube(edict_t *best);

/*
 * This is a support routine used when a client is firing
 * a non-instant attack weapon.  It checks to see if a
 * monster's dodge function should be called.
 */
void
check_dodge(edict_t *self, vec3_t start, vec3_t dir, int speed)
{
	vec3_t end;
	vec3_t v;
	trace_t tr;
	float eta;

	if (!self)
	{
		return;
	}

	/* easy mode only ducks one quarter the time */
	if (skill->value == SKILL_EASY)
	{
		if (random() > 0.25)
		{
			return;
		}
	}

	VectorMA(start, 8192, dir, end);
	tr = gi.trace(start, NULL, NULL, end, self, MASK_SHOT);

	if ((tr.ent) && (tr.ent->svflags & SVF_MONSTER) && (tr.ent->health > 0) &&
		(tr.ent->monsterinfo.dodge) && infront(tr.ent, self))
	{
		VectorSubtract(tr.endpos, start, v);
		eta = (VectorLength(v) - tr.ent->maxs[0]) / speed;
		tr.ent->monsterinfo.dodge(tr.ent, self, eta);
	}
}

/*
 * Used for all impact (hit/punch/slash) attacks
 */
qboolean
fire_hit(edict_t *self, vec3_t aim, int damage, int kick)
{
	trace_t tr;
	vec3_t forward, right, up;
	vec3_t v;
	vec3_t point;
	float range;
	vec3_t dir;

	if (!self)
	{
		return false;
	}

	if (!self->enemy)
	{
		return false;
	}

	/* see if enemy is in range */
	VectorSubtract(self->enemy->s.origin, self->s.origin, dir);
	range = VectorLength(dir);

	if (range > aim[0])
	{
		return false;
	}

	if ((aim[1] > self->mins[0]) && (aim[1] < self->maxs[0]))
	{
		/* the hit is straight on so back the range up to the edge of their bbox */
		range -= self->enemy->maxs[0];
	}
	else
	{
		/* this is a side hit so adjust the "right" value out to the edge of their bbox */
		if (aim[1] < 0)
		{
			aim[1] = self->enemy->mins[0];
		}
		else
		{
			aim[1] = self->enemy->maxs[0];
		}
	}

	VectorMA(self->s.origin, range, dir, point);

	tr = gi.trace(self->s.origin, NULL, NULL, point, self, MASK_SHOT);

	if (tr.fraction < 1)
	{
		if (!tr.ent->takedamage)
		{
			return false;
		}

		/* if it will hit any client/monster then hit the one we wanted to hit */
		if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client))
		{
			tr.ent = self->enemy;
		}
	}

	AngleVectors(self->s.angles, forward, right, up);
	VectorMA(self->s.origin, range, forward, point);
	VectorMA(point, aim[1], right, point);
	VectorMA(point, aim[2], up, point);
	VectorSubtract(point, self->enemy->s.origin, dir);

	/* do the damage */
	T_Damage(tr.ent, self, self, dir, point, vec3_origin,
			damage, kick / 2, DAMAGE_NO_KNOCKBACK, MOD_HIT);

	if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
	{
		return false;
	}

	/* do our special form of knockback here */
	VectorMA(self->enemy->absmin, 0.5, self->enemy->size, v);
	VectorSubtract(v, point, v);
	VectorNormalize(v);
	VectorMA(self->enemy->velocity, kick, v, self->enemy->velocity);

	if (self->enemy->velocity[2] > 0)
	{
		self->enemy->groundentity = NULL;
	}

	return true;
}

/*
 * This is an internal support routine used for bullet/pellet based weapons.
 */
void
fire_lead(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick,
		int te_impact, int hspread, int vspread, int mod)
{
	trace_t tr;
	vec3_t dir;
	vec3_t forward, right, up;
	vec3_t end;
	float r;
	float u;
	vec3_t water_start;
	qboolean water = false;
	int content_mask = MASK_SHOT | MASK_WATER;

	if (!self)
	{
		return;
	}

	tr = gi.trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);

	if (!(tr.fraction < 1.0))
	{
		vectoangles(aimdir, dir);
		AngleVectors(dir, forward, right, up);

		r = crandom() * hspread;
		u = crandom() * vspread;
		VectorMA(start, 8192, forward, end);
		VectorMA(end, r, right, end);
		VectorMA(end, u, up, end);

		if (gi.pointcontents(start) & MASK_WATER)
		{
			water = true;
			VectorCopy(start, water_start);
			content_mask &= ~MASK_WATER;
		}

		tr = gi.trace(start, NULL, NULL, end, self, content_mask);

		/* see if we hit water */
		if (tr.contents & MASK_WATER)
		{
			int color;

			water = true;
			VectorCopy(tr.endpos, water_start);

			if (!VectorCompare(start, tr.endpos))
			{
				if (tr.contents & CONTENTS_WATER)
				{
					if (strcmp(tr.surface->name, "*brwater") == 0)
					{
						color = SPLASH_BROWN_WATER;
					}
					else
					{
						color = SPLASH_BLUE_WATER;
					}
				}
				else if (tr.contents & CONTENTS_SLIME)
				{
					color = SPLASH_SLIME;
				}
				else if (tr.contents & CONTENTS_LAVA)
				{
					color = SPLASH_LAVA;
				}
				else
				{
					color = SPLASH_UNKNOWN;
				}

				if (color != SPLASH_UNKNOWN)
				{
					gi.WriteByte(svc_temp_entity);
					gi.WriteByte(TE_SPLASH);
					gi.WriteByte(8);
					gi.WritePosition(tr.endpos);
					gi.WriteDir(tr.plane.normal);
					gi.WriteByte(color);
					gi.multicast(tr.endpos, MULTICAST_PVS);
				}

				/* change bullet's course when it enters water */
				VectorSubtract(end, start, dir);
				vectoangles(dir, dir);
				AngleVectors(dir, forward, right, up);
				r = crandom() * hspread * 2;
				u = crandom() * vspread * 2;
				VectorMA(water_start, 8192, forward, end);
				VectorMA(end, r, right, end);
				VectorMA(end, u, up, end);
			}

			/* re-trace ignoring water this time */
			tr = gi.trace(water_start, NULL, NULL, end, self, MASK_SHOT);
		}
	}

	/* send gun puff / flash */
	if (!((tr.surface) && (tr.surface->flags & SURF_SKY)))
	{
		if (tr.fraction < 1.0)
		{
			if (tr.ent->takedamage)
			{
				T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal,
						damage, kick, DAMAGE_BULLET, mod);
			}
			else
			{
				if (strncmp(tr.surface->name, "sky", 3) != 0)
				{
					gi.WriteByte(svc_temp_entity);
					gi.WriteByte(te_impact);
					gi.WritePosition(tr.endpos);
					gi.WriteDir(tr.plane.normal);
					gi.multicast(tr.endpos, MULTICAST_PVS);

					if (self->client)
					{
						PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
					}
				}
			}
		}
	}

	/* if went through water, determine where the end and make a bubble trail */
	if (water)
	{
		vec3_t pos;

		VectorSubtract(tr.endpos, water_start, dir);
		VectorNormalize(dir);
		VectorMA(tr.endpos, -2, dir, pos);

		if (gi.pointcontents(pos) & MASK_WATER)
		{
			VectorCopy(pos, tr.endpos);
		}
		else
		{
			tr = gi.trace(pos, NULL, NULL, water_start, tr.ent, MASK_WATER);
		}

		VectorAdd(water_start, tr.endpos, pos);
		VectorScale(pos, 0.5, pos);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BUBBLETRAIL);
		gi.WritePosition(water_start);
		gi.WritePosition(tr.endpos);
		gi.multicast(pos, MULTICAST_PVS);
	}
}

/*
 * Fires a single round. Used for machinegun and chaingun.
 * Would be fine for pistols, rifles, etc....
 */
void
fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage,
		int kick, int hspread, int vspread, int mod)
{
	if (!self)
	{
		return;
	}

	fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT,
			hspread, vspread, mod);
}

/*
 * Shoots shotgun pellets. Used by shotgun and super shotgun.
 */
void
fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage,
		int kick, int hspread, int vspread, int count, int mod)
{
	int i;

	if (!self)
	{
		return;
	}

	for (i = 0; i < count; i++)
	{
		fire_lead(self, start, aimdir, damage, kick,
				TE_SHOTGUN, hspread, vspread, mod);
	}
}

/*
 * Fires a single blaster bolt. Used by the blaster and hyper blaster.
 */
void
blaster_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	int mod;
	vec3_t normal;

	if (!self || !other)
	{
		G_FreeEdict(self);
		return;
	}

	if (other == self->owner)
	{
		return;
	}

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	if (self->owner->client)
	{
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);
	}

	get_normal_vector(plane, normal);

	if (other->takedamage)
	{
		if (self->spawnflags & 1)
		{
			mod = MOD_HYPERBLASTER;
		}
		else
		{
			mod = MOD_BLASTER;
		}

		T_Damage(other, self, self->owner, self->velocity, self->s.origin,
				normal, self->dmg, 1, DAMAGE_ENERGY, mod);
	}
	else
	{
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BLASTER);
		gi.WritePosition(self->s.origin);
		gi.WriteDir(normal);
		gi.multicast(self->s.origin, MULTICAST_PVS);
	}

	G_FreeEdict(self);
}

void
fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage,
		int speed, int effect, qboolean hyper)
{
	edict_t *bolt;
	trace_t tr;

	if (!self)
	{
		return;
	}

	VectorNormalize(dir);

	bolt = G_Spawn();
	bolt->svflags = SVF_DEADMONSTER;

	/* yes, I know it looks weird that projectiles are deadmonsters
	   what this means is that when prediction is used against the object
	   (blaster/hyperblaster shots), the player won't be solid clipped against
	   the object.  Right now trying to run into a firing hyperblaster
	   is very jerky since you are predicted 'against' the shots. */
	VectorCopy(start, bolt->s.origin);
	VectorCopy(start, bolt->s.old_origin);
	vectoangles(dir, bolt->s.angles);
	VectorScale(dir, speed, bolt->velocity);
	bolt->movetype = MOVETYPE_FLYMISSILE;
	bolt->clipmask = MASK_SHOT;
	bolt->solid = SOLID_BBOX;
	bolt->s.effects |= effect;
	VectorClear(bolt->mins);
	VectorClear(bolt->maxs);
	bolt->s.modelindex = gi.modelindex("models/objects/laser/tris.md2");
	bolt->s.sound = gi.soundindex("misc/lasfly.wav");
	bolt->owner = self;
	bolt->touch = blaster_touch;
	bolt->nextthink = level.time + 2;
	bolt->think = G_FreeEdict;
	bolt->dmg = damage;
	bolt->classname = "bolt";

	if (hyper)
	{
		bolt->spawnflags = 1;
	}

	gi.linkentity(bolt);

	if (self->client)
	{
		check_dodge(self, bolt->s.origin, dir, speed);
	}

	tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);

	if (tr.fraction < 1.0)
	{
		VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
		bolt->touch(bolt, tr.ent, NULL, NULL);
	}
}

void
fire_blueblaster(edict_t *self, vec3_t start, vec3_t dir, int damage,
		int speed, int effect)
{
	edict_t *bolt;
	trace_t tr;

	if (!self)
	{
		return;
	}

	VectorNormalize(dir);

	bolt = G_Spawn();
	VectorCopy(start, bolt->s.origin);
	VectorCopy(start, bolt->s.old_origin);
	vectoangles(dir, bolt->s.angles);
	VectorScale(dir, speed, bolt->velocity);
	bolt->movetype = MOVETYPE_FLYMISSILE;
	bolt->clipmask = MASK_SHOT;
	bolt->solid = SOLID_BBOX;
	bolt->s.effects |= effect;
	VectorClear(bolt->mins);
	VectorClear(bolt->maxs);

	bolt->s.modelindex = gi.modelindex("models/objects/blaser/tris.md2");
	bolt->s.sound = gi.soundindex("misc/lasfly.wav");
	bolt->owner = self;
	bolt->touch = blaster_touch;
	bolt->nextthink = level.time + 2;
	bolt->think = G_FreeEdict;
	bolt->dmg = damage;
	bolt->classname = "bolt";
	gi.linkentity(bolt);

	if (self->client)
	{
		check_dodge(self, bolt->s.origin, dir, speed);
	}

	tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);

	if (tr.fraction < 1.0)
	{
		VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
		bolt->touch(bolt, tr.ent, NULL, NULL);
	}
}

void
Grenade_Explode(edict_t *ent)
{
	vec3_t origin;
	int mod;

	if (!ent)
	{
		return;
	}

	if (ent->owner->client)
	{
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);
	}

	if (ent->enemy)
	{
		float points;
		vec3_t v;
		vec3_t dir;

		VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
		VectorMA(ent->enemy->s.origin, 0.5, v, v);
		VectorSubtract(ent->s.origin, v, v);
		points = ent->dmg - 0.5 * VectorLength(v);
		VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);

		if (ent->spawnflags & 1)
		{
			mod = MOD_HANDGRENADE;
		}
		else
		{
			mod = MOD_GRENADE;
		}

		T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin,
				(int)points, (int)points, DAMAGE_RADIUS, mod);
	}

	if (ent->spawnflags & 2)
	{
		mod = MOD_HELD_GRENADE;
	}
	else if (ent->spawnflags & 1)
	{
		mod = MOD_HG_SPLASH;
	}
	else
	{
		mod = MOD_G_SPLASH;
	}

	T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

	VectorMA(ent->s.origin, -0.02, ent->velocity, origin);
	gi.WriteByte(svc_temp_entity);

	if (ent->waterlevel)
	{
		if (ent->groundentity)
		{
			gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
		}
		else
		{
			gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
		}
	}
	else
	{
		if (ent->groundentity)
		{
			gi.WriteByte(TE_GRENADE_EXPLOSION);
		}
		else
		{
			gi.WriteByte(TE_ROCKET_EXPLOSION);
		}
	}

	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS);

	G_FreeEdict(ent);
}

void
Grenade_Touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (!ent || !other)
	{
		G_FreeEdict(ent);
		return;
	}

	if (other == ent->owner)
	{
		return;
	}

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (!other->takedamage)
	{
		if (ent->spawnflags & 1)
		{
			if (random() > 0.5)
			{
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"),
					   	1, ATTN_NORM, 0);
			}
			else
			{
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"),
					   	1, ATTN_NORM, 0);
			}
		}
		else
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"),
				   	1, ATTN_NORM, 0);
		}

		return;
	}

	ent->enemy = other;
	Grenade_Explode(ent);
}

void
fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage,
		int speed, float timer, float damage_radius)
{
	edict_t *grenade;
	vec3_t dir;
	vec3_t forward, right, up;

	if (!self)
	{
		return;
	}

	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy(start, grenade->s.origin);
	VectorScale(aimdir, speed, grenade->velocity);
	VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
	VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
	VectorSet(grenade->avelocity, 300, 300, 300);
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_SHOT;
	grenade->solid = SOLID_BBOX;
	grenade->s.effects |= EF_GRENADE;
	VectorClear(grenade->mins);
	VectorClear(grenade->maxs);
	grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
	grenade->owner = self;
	grenade->touch = Grenade_Touch;
	grenade->nextthink = level.time + timer;
	grenade->think = Grenade_Explode;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "grenade";

	gi.linkentity(grenade);
}

void
fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage,
		int speed, float timer, float damage_radius, qboolean held)
{
	edict_t *grenade;
	vec3_t dir;
	vec3_t forward, right, up;

	if (!self)
	{
		return;
	}

	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy(start, grenade->s.origin);
	VectorScale(aimdir, speed, grenade->velocity);
	VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
	VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
	VectorSet(grenade->avelocity, 300, 300, 300);
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_SHOT;
	grenade->solid = SOLID_BBOX;
	grenade->s.effects |= EF_GRENADE;
	VectorClear(grenade->mins);
	VectorClear(grenade->maxs);
	grenade->s.modelindex = gi.modelindex("models/objects/grenade2/tris.md2");
	grenade->owner = self;
	grenade->touch = Grenade_Touch;
	grenade->nextthink = level.time + timer;
	grenade->think = Grenade_Explode;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "hgrenade";

	if (held)
	{
		grenade->spawnflags = 3;
	}
	else
	{
		grenade->spawnflags = 1;
	}

	grenade->s.sound = gi.soundindex("weapons/hgrenc1b.wav");

	if (timer <= 0.0)
	{
		Grenade_Explode(grenade);
	}
	else
	{
		gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/hgrent1a.wav"),
			   	1, ATTN_NORM, 0);
		gi.linkentity(grenade);
	}
}

void
rocket_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t origin;
	vec3_t normal;
	int n;

	if (!ent || !other)
	{
		G_FreeEdict(ent);
		return;
	}

	if (other == ent->owner)
	{
		return;
	}

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (ent->owner->client)
	{
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);
	}

	/* calculate position for the explosion entity */
	VectorMA(ent->s.origin, -0.02, ent->velocity, origin);

	if (other->takedamage)
	{
		get_normal_vector(plane, normal);

		T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin,
				normal, ent->dmg, 0, 0, MOD_ROCKET);
	}
	else
	{
		/* don't throw any debris in net games */
		if (!deathmatch->value && !coop->value)
		{
			if ((surf) && !(surf->flags &
				(SURF_WARP | SURF_TRANS33 | SURF_TRANS66 | SURF_FLOWING)))
			{
				n = rand() % 5;

				while (n--)
				{
					ThrowDebris(ent, "models/objects/debris2/tris.md2",
							2, ent->s.origin);
				}
			}
		}
	}

	T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other,
			ent->dmg_radius, MOD_R_SPLASH);

	gi.WriteByte(svc_temp_entity);

	if (ent->waterlevel)
	{
		gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
	}
	else
	{
		gi.WriteByte(TE_ROCKET_EXPLOSION);
	}

	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS);

	G_FreeEdict(ent);
}

void
fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage,
		int speed, float damage_radius, int radius_damage)
{
	edict_t *rocket;

	if (!self)
	{
		return;
	}

	rocket = G_Spawn();
	VectorCopy(start, rocket->s.origin);
	VectorCopy(dir, rocket->movedir);
	vectoangles(dir, rocket->s.angles);
	VectorScale(dir, speed, rocket->velocity);
	rocket->movetype = MOVETYPE_FLYMISSILE;
	rocket->clipmask = MASK_SHOT;
	rocket->solid = SOLID_BBOX;
	rocket->s.effects |= EF_ROCKET;
	VectorClear(rocket->mins);
	VectorClear(rocket->maxs);
	rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
	rocket->owner = self;
	rocket->touch = rocket_touch;
	rocket->nextthink = level.time + (8000.0f / (float)speed);
	rocket->think = G_FreeEdict;
	rocket->dmg = damage;
	rocket->radius_dmg = radius_damage;
	rocket->dmg_radius = damage_radius;
	rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
	rocket->classname = "rocket";

	if (self->client)
	{
		check_dodge(self, rocket->s.origin, dir, speed);
	}

	gi.linkentity(rocket);
}

void
fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
	vec3_t from;
	vec3_t end;
	trace_t tr;
	edict_t *ignore;
	int mask;
	qboolean water;

	if (!self)
	{
		return;
	}

	VectorMA(start, 8192, aimdir, end);
	VectorCopy(start, from);
	ignore = self;
	water = false;
	mask = MASK_SHOT | CONTENTS_SLIME | CONTENTS_LAVA;

	while (ignore)
	{
		tr = gi.trace(from, NULL, NULL, end, ignore, mask);

		if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA))
		{
			mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
			water = true;
		}
		else
		{
			/* -added so rail goes through SOLID_BBOX entities (gibs, etc) */
			if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
				(tr.ent->solid == SOLID_BBOX))
			{
				ignore = tr.ent;
			}
			else
			{
				ignore = NULL;
			}

			if ((tr.ent != self) && (tr.ent->takedamage))
			{
				T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal,
						damage, kick, 0, MOD_RAILGUN);
			}
			else
			{
				ignore = NULL;
			}
		}

		VectorCopy(tr.endpos, from);
	}

	/* send gun puff / flash */
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_RAILTRAIL);
	gi.WritePosition(start);
	gi.WritePosition(tr.endpos);
	gi.multicast(self->s.origin, MULTICAST_PHS);

	if (water)
	{
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_RAILTRAIL);
		gi.WritePosition(start);
		gi.WritePosition(tr.endpos);
		gi.multicast(tr.endpos, MULTICAST_PHS);
	}

	if (self->client)
	{
		PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
	}
}

void
bfg_explode(edict_t *self)
{
	edict_t *ent;
	float points;
	vec3_t v;
	float dist;

	if (!self)
	{
		return;
	}

	if (self->s.frame == 0)
	{
		/* the BFG effect */
		ent = NULL;

		while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL)
		{
			if (!ent->takedamage)
			{
				continue;
			}

			if (ent == self->owner)
			{
				continue;
			}

			if (!CanDamage(ent, self))
			{
				continue;
			}

			if (!CanDamage(ent, self->owner))
			{
				continue;
			}

			VectorAdd(ent->mins, ent->maxs, v);
			VectorMA(ent->s.origin, 0.5, v, v);
			VectorSubtract(self->s.origin, v, v);
			dist = VectorLength(v);
			points = self->radius_dmg * (1.0 - sqrt(dist / self->dmg_radius));

			if (ent == self->owner)
			{
				points = points * 0.5;
			}

			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_BFG_EXPLOSION);
			gi.WritePosition(ent->s.origin);
			gi.multicast(ent->s.origin, MULTICAST_PHS);
			T_Damage(ent, self, self->owner, self->velocity, ent->s.origin,
					vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
		}
	}

	self->nextthink = level.time + FRAMETIME;
	self->s.frame++;

	if (self->s.frame == 5)
	{
		self->think = G_FreeEdict;
	}
}

void
bfg_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t normal;

	if (!self || !other)
	{
		G_FreeEdict(self);
		return;
	}

	if (other == self->owner)
	{
		return;
	}

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	if (self->owner->client)
	{
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);
	}

	/* core explosion - prevents firing it into the wall/floor */
	if (other->takedamage)
	{
		get_normal_vector(plane, normal);

		T_Damage(other, self, self->owner, self->velocity, self->s.origin,
				normal, 200, 0, 0, MOD_BFG_BLAST);
	}

	T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);

	gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
	self->solid = SOLID_NOT;
	self->touch = NULL;

	/* move it back a bit from walls so the effects aren't cut off */
	if (!other->takedamage)
	{
		VectorNormalize(self->velocity);
		VectorMA(self->s.origin, -40.0f, self->velocity, self->s.origin);
	}

	VectorClear(self->velocity);
	self->s.modelindex = gi.modelindex("sprites/s_bfg3.sp2");
	self->s.frame = 0;
	self->s.sound = 0;
	self->s.effects &= ~EF_ANIM_ALLFAST;
	self->think = bfg_explode;
	self->nextthink = level.time + FRAMETIME;
	self->enemy = other;

	gi.linkentity(self);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BFG_BIGEXPLOSION);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PVS);
}

void
bfg_think(edict_t *self)
{
	edict_t *ent;
	edict_t *ignore;
	vec3_t point;
	vec3_t dir;
	vec3_t start;
	vec3_t end;
	int dmg;
	trace_t tr;

	if (!self)
	{
		return;
	}

	if (deathmatch->value)
	{
		dmg = 5;
	}
	else
	{
		dmg = 10;
	}

	ent = NULL;

	while ((ent = findradius(ent, self->s.origin, 256)) != NULL)
	{
		if (ent == self)
		{
			continue;
		}

		if (ent == self->owner)
		{
			continue;
		}

		if (!ent->takedamage)
		{
			continue;
		}

		if (!(ent->svflags & SVF_MONSTER) && (!ent->client) &&
			(strcmp(ent->classname, "misc_explobox") != 0))
		{
			continue;
		}

		VectorMA(ent->absmin, 0.5, ent->size, point);

		VectorSubtract(point, self->s.origin, dir);
		VectorNormalize(dir);

		ignore = self;
		VectorCopy(self->s.origin, start);
		VectorMA(start, 2048, dir, end);

		while (1)
		{
			tr = gi.trace(start, NULL, NULL, end, ignore,
					CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

			if (!tr.ent)
			{
				break;
			}

			/* hurt it if we can */
			if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) &&
				(tr.ent != self->owner))
			{
				T_Damage(tr.ent, self, self->owner, dir, tr.endpos,
						vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);
			}

			/* if we hit something that's not a monster or player we're done */
			if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
			{
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_LASER_SPARKS);
				gi.WriteByte(4);
				gi.WritePosition(tr.endpos);
				gi.WriteDir(tr.plane.normal);
				gi.WriteByte(self->s.skinnum);
				gi.multicast(tr.endpos, MULTICAST_PVS);
				break;
			}

			ignore = tr.ent;
			VectorCopy(tr.endpos, start);
		}

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BFG_LASER);
		gi.WritePosition(self->s.origin);
		gi.WritePosition(tr.endpos);
		gi.multicast(self->s.origin, MULTICAST_PHS);
	}

	self->nextthink = level.time + FRAMETIME;
}

void
fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage,
		int speed, float damage_radius)
{
	edict_t *bfg;

	if (!self)
	{
		return;
	}

	bfg = G_Spawn();
	VectorCopy(start, bfg->s.origin);
	VectorCopy(dir, bfg->movedir);
	vectoangles(dir, bfg->s.angles);
	VectorScale(dir, speed, bfg->velocity);
	bfg->movetype = MOVETYPE_FLYMISSILE;
	bfg->clipmask = MASK_SHOT;
	bfg->solid = SOLID_BBOX;
	bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
	VectorClear(bfg->mins);
	VectorClear(bfg->maxs);
	bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
	bfg->owner = self;
	bfg->touch = bfg_touch;
	bfg->nextthink = level.time + (8000.0f / (float)speed);
	bfg->think = G_FreeEdict;
	bfg->radius_dmg = damage;
	bfg->dmg_radius = damage_radius;
	bfg->classname = "bfg blast";
	bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");

	bfg->think = bfg_think;
	bfg->nextthink = level.time + FRAMETIME;
	bfg->teammaster = bfg;
	bfg->teamchain = NULL;

	if (self->client)
	{
		check_dodge(self, bfg->s.origin, dir, speed);
	}

	gi.linkentity(bfg);
}

void
ionripper_sparks(edict_t *self)
{
	if (!self)
	{
		return;
	}

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_WELDING_SPARKS);
	gi.WriteByte(0);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(vec3_origin);
	gi.WriteByte(0xe4 + (rand() & 3));
	gi.multicast(self->s.origin, MULTICAST_PVS);

	G_FreeEdict(self);
}

void
ionripper_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t normal;

	if (!self || !other)
	{
		return;
	}

	if (other == self->owner)
	{
		return;
	}

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	if (self->owner->client)
	{
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);
	}

	if (other->takedamage)
	{
		get_normal_vector(plane, normal);

		T_Damage(other, self, self->owner, self->velocity, self->s.origin,
				normal, self->dmg, 1, DAMAGE_ENERGY, MOD_RIPPER);

		G_FreeEdict(self);
	}
}

void
fire_ionripper(edict_t *self, vec3_t start, vec3_t dir, int damage,
		int speed, int effect)
{
	edict_t *ion;
	trace_t tr;

	if (!self)
	{
		return;
	}

	VectorNormalize(dir);

	ion = G_Spawn();
	VectorCopy(start, ion->s.origin);
	VectorCopy(start, ion->s.old_origin);
	vectoangles(dir, ion->s.angles);
	VectorScale(dir, speed, ion->velocity);

	ion->movetype = MOVETYPE_WALLBOUNCE;
	ion->clipmask = MASK_SHOT;
	ion->solid = SOLID_BBOX;
	ion->s.effects |= effect;

	ion->s.renderfx |= RF_FULLBRIGHT;

	VectorClear(ion->mins);
	VectorClear(ion->maxs);
	ion->s.modelindex = gi.modelindex("models/objects/boomrang/tris.md2");
	ion->s.sound = gi.soundindex("misc/lasfly.wav");
	ion->owner = self;
	ion->touch = ionripper_touch;
	ion->nextthink = level.time + 3;
	ion->think = ionripper_sparks;
	ion->dmg = damage;
	ion->dmg_radius = 100;
	gi.linkentity(ion);

	if (self->client)
	{
		check_dodge(self, ion->s.origin, dir, speed);
	}

	tr = gi.trace(self->s.origin, NULL, NULL, ion->s.origin, ion, MASK_SHOT);

	if (tr.fraction < 1.0)
	{
		VectorMA(ion->s.origin, -10, dir, ion->s.origin);
		ion->touch(ion, tr.ent, NULL, NULL);
	}
}

void
heat_think(edict_t *self)
{
	edict_t *target = NULL;
	edict_t *aquire = NULL;
	vec3_t vec;
	float len;
	float oldlen = 0;

	if (!self)
	{
		return;
	}

	/* aquire new target */
	while ((target = findradius(target, self->s.origin, 1024)) != NULL)
	{
		if (self->owner == target)
		{
			continue;
		}

		if (!target->client)
		{
			continue;
		}

		if (target->health <= 0)
		{
			continue;
		}

		if (!infront(self, target))
		{
			continue;
		}

		if (!visible(self, target))
		{
			continue;
		}

		VectorSubtract(self->s.origin, target->s.origin, vec);
		len = VectorLength(vec);

		if ((!aquire) || (len < oldlen))
		{
			aquire = target;
			oldlen = len;
		}
	}

	if (aquire)
	{
		VectorSubtract(aquire->s.origin, self->s.origin, vec);
		vectoangles(vec, self->s.angles);
		VectorNormalize(vec);
		VectorScale(vec, 500, self->velocity);
	}

	self->nextthink = level.time + 0.1;
}

void
fire_heat(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed,
		float damage_radius, int radius_damage)
{
	edict_t *heat;

	if (!self)
	{
		return;
	}

	heat = G_Spawn();
	VectorCopy(start, heat->s.origin);
	VectorCopy(dir, heat->movedir);
	vectoangles(dir, heat->s.angles);
	VectorScale(dir, speed, heat->velocity);
	heat->movetype = MOVETYPE_FLYMISSILE;
	heat->clipmask = MASK_SHOT;
	heat->solid = SOLID_BBOX;
	heat->s.effects |= EF_ROCKET;
	VectorClear(heat->mins);
	VectorClear(heat->maxs);
	heat->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
	heat->owner = self;
	heat->touch = rocket_touch;

	heat->nextthink = level.time + 0.1;
	heat->think = heat_think;

	heat->dmg = damage;
	heat->radius_dmg = radius_damage;
	heat->dmg_radius = damage_radius;
	heat->s.sound = gi.soundindex("weapons/rockfly.wav");

	if (self->client)
	{
		check_dodge(self, heat->s.origin, dir, speed);
	}

	gi.linkentity(heat);
}

void
plasma_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t origin;
	vec3_t normal;

	if (!ent || !other)
	{
		return;
	}
	
	if (other == ent->owner)
	{
		return;
	}

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (ent->owner->client)
	{
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);
	}

	/* calculate position for the explosion entity */
	VectorMA(ent->s.origin, -0.02, ent->velocity, origin);

	if (other->takedamage)
	{
		get_normal_vector(plane, normal);

		T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin,
				normal, ent->dmg, 0, 0, MOD_PHALANX);
	}

	T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other,
			ent->dmg_radius, MOD_PHALANX);

	gi.WriteByte(svc_temp_entity);

	if (ent->waterlevel)
	{
		gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
	}
	else
	{
		gi.WriteByte(TE_PLASMA_EXPLOSION);
	}

	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PVS);

	G_FreeEdict(ent);
}

void
fire_plasma(edict_t *self, vec3_t start, vec3_t dir, int damage,
		int speed, float damage_radius, int radius_damage)
{
	edict_t *plasma;

	if (!self)
	{
		return;
	}

	plasma = G_Spawn();
	VectorCopy(start, plasma->s.origin);
	VectorCopy(dir, plasma->movedir);
	vectoangles(dir, plasma->s.angles);
	VectorScale(dir, speed, plasma->velocity);
	plasma->movetype = MOVETYPE_FLYMISSILE;
	plasma->clipmask = MASK_SHOT;
	plasma->solid = SOLID_BBOX;

	VectorClear(plasma->mins);
	VectorClear(plasma->maxs);

	plasma->owner = self;
	plasma->touch = plasma_touch;
	plasma->nextthink = level.time + (8000.0f / (float)speed);
	plasma->think = G_FreeEdict;
	plasma->dmg = damage;
	plasma->radius_dmg = radius_damage;
	plasma->dmg_radius = damage_radius;
	plasma->s.sound = gi.soundindex("weapons/rockfly.wav");

	plasma->s.modelindex = gi.modelindex("sprites/s_photon.sp2");
	plasma->s.effects |= EF_PLASMA | EF_ANIM_ALLFAST;

	if (self->client)
	{
		check_dodge(self, plasma->s.origin, dir, speed);
	}

	gi.linkentity(plasma);
}

void
Trap_Think(edict_t *ent)
{
	edict_t *target = NULL;
	edict_t *best = NULL;
	vec3_t vec;
	int len, i;
	int oldlen = 8000;
	vec3_t forward, right, up;

	if (!ent)
	{
		return;
	}

	if (ent->timestamp < level.time)
	{
		BecomeExplosion1(ent);
		return;
	}

	ent->nextthink = level.time + 0.1;

	if (!ent->groundentity)
	{
		return;
	}

	/* ok lets do the blood effect */
	if (ent->s.frame > 4)
	{
		if (ent->s.frame == 5)
		{
			if (ent->wait == 64)
			{
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/trapdown.wav"),
					   	1, ATTN_IDLE, 0);
			}

			ent->wait -= 2;
			ent->delay += level.time;

			for (i = 0; i < 3; i++)
			{
				best = G_Spawn();

				if (strcmp(ent->enemy->classname, "monster_gekk") == 0)
				{
					best->s.modelindex = gi.modelindex("models/objects/gekkgib/torso/tris.md2");
					best->s.effects |= TE_GREENBLOOD;
				}
				else if (ent->mass > 200)
				{
					best->s.modelindex = gi.modelindex("models/objects/gibs/chest/tris.md2");
					best->s.effects |= TE_BLOOD;
				}
				else
				{
					best->s.modelindex = gi.modelindex("models/objects/gibs/sm_meat/tris.md2");
					best->s.effects |= TE_BLOOD;
				}

				AngleVectors(ent->s.angles, forward, right, up);

				RotatePointAroundVector(vec, up, right, ((360.0 / 3) * i) + ent->delay);
				VectorMA(vec, ent->wait / 2, vec, vec);
				VectorAdd(vec, ent->s.origin, vec);
				VectorAdd(vec, forward, best->s.origin);

				best->s.origin[2] = ent->s.origin[2] + ent->wait;

				VectorCopy(ent->s.angles, best->s.angles);

				best->solid = SOLID_NOT;
				best->s.effects |= EF_GIB;
				best->takedamage = DAMAGE_YES;

				best->movetype = MOVETYPE_TOSS;
				best->svflags |= SVF_MONSTER;
				best->deadflag = DEAD_DEAD;

				VectorClear(best->mins);
				VectorClear(best->maxs);

				best->watertype = gi.pointcontents(best->s.origin);

				if (best->watertype & MASK_WATER)
				{
					best->waterlevel = 1;
				}

				best->nextthink = level.time + 0.1;
				best->think = G_FreeEdict;
				gi.linkentity(best);
			}

			if (ent->wait < 19)
			{
				ent->s.frame++;
			}

			return;
		}

		ent->s.frame++;

		if (ent->s.frame == 8)
		{
			ent->nextthink = level.time + 1.0;
			ent->think = G_FreeEdict;

			best = G_Spawn();
			SP_item_foodcube(best);
			VectorCopy(ent->s.origin, best->s.origin);
			best->s.origin[2] += 16;
			best->velocity[2] = 400;
			best->count = ent->mass;
			gi.linkentity(best);
			return;
		}

		return;
	}

	ent->s.effects &= ~EF_TRAP;

	if (ent->s.frame >= 4)
	{
		ent->s.effects |= EF_TRAP;
		VectorClear(ent->mins);
		VectorClear(ent->maxs);
	}

	if (ent->s.frame < 4)
	{
		ent->s.frame++;
	}

	while ((target = findradius(target, ent->s.origin, 256)) != NULL)
	{
		if (target == ent)
		{
			continue;
		}

		if (!(target->svflags & SVF_MONSTER) && !target->client)
		{
			continue;
		}

		if (target->health <= 0)
		{
			continue;
		}

		if (!visible(ent, target))
		{
			continue;
		}

		if (!best)
		{
			best = target;
			continue;
		}

		VectorSubtract(ent->s.origin, target->s.origin, vec);
		len = VectorLength(vec);

		if (len < oldlen)
		{
			oldlen = len;
			best = target;
		}
	}

	/* pull the enemy in */
	if (best)
	{
		vec3_t forward;

		if (best->groundentity)
		{
			best->s.origin[2] += 1;
			best->groundentity = NULL;
		}

		VectorSubtract(ent->s.origin, best->s.origin, vec);
		len = VectorLength(vec);

		if (best->client)
		{
			VectorNormalize(vec);
			VectorMA(best->velocity, 250, vec, best->velocity);
		}
		else
		{
			best->ideal_yaw = vectoyaw(vec);
			M_ChangeYaw(best);
			AngleVectors(best->s.angles, forward, NULL, NULL);
			VectorScale(forward, 256, best->velocity);
		}

		gi.sound(ent, CHAN_VOICE, gi.soundindex(
						"weapons/trapsuck.wav"), 1, ATTN_IDLE, 0);

		if (len < 32)
		{
			if (best->mass < 400)
			{
				T_Damage(best, ent, ent->owner, vec3_origin, best->s.origin,
						vec3_origin, 100000, 1, 0, MOD_TRAP);
				ent->enemy = best;
				ent->wait = 64;
				VectorCopy(ent->s.origin, ent->s.old_origin);
				ent->timestamp = level.time + 30;

				if (deathmatch->value)
				{
					ent->mass = best->mass / 4;
				}
				else
				{
					ent->mass = best->mass / 10;
				}

				/* ok spawn the food cube */
				ent->s.frame = 5;
			}
			else
			{
				BecomeExplosion1(ent);
				return;
			}
		}
	}
}

void
fire_trap(edict_t *self, vec3_t start, vec3_t aimdir, int damage,
		int speed, float timer, float damage_radius, qboolean held)
{
	edict_t *trap;
	vec3_t dir;
	vec3_t forward, right, up;

	if (!self)
	{
		return;
	}

	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	trap = G_Spawn();
	VectorCopy(start, trap->s.origin);
	VectorScale(aimdir, speed, trap->velocity);
	VectorMA(trap->velocity, 200 + crandom() * 10.0, up, trap->velocity);
	VectorMA(trap->velocity, crandom() * 10.0, right, trap->velocity);
	VectorSet(trap->avelocity, 0, 300, 0);
	trap->movetype = MOVETYPE_BOUNCE;
	trap->clipmask = MASK_SHOT;
	trap->solid = SOLID_BBOX;
	VectorSet(trap->mins, -4, -4, 0);
	VectorSet(trap->maxs, 4, 4, 8);
	trap->s.modelindex = gi.modelindex("models/weapons/z_trap/tris.md2");
	trap->owner = self;
	trap->nextthink = level.time + 1.0;
	trap->think = Trap_Think;
	trap->dmg = damage;
	trap->dmg_radius = damage_radius;
	trap->classname = "htrap";
	trap->s.sound = gi.soundindex("weapons/traploop.wav");

	if (held)
	{
		trap->spawnflags = 3;
	}
	else
	{
		trap->spawnflags = 1;
	}

	if (timer <= 0.0)
	{
		Grenade_Explode(trap);
	}
	else
	{
		gi.linkentity(trap);
	}

	trap->timestamp = level.time + 30;
}

/*
 * Drops a spark from the flare flying thru the air.  Checks to make
 * sure we aren't in the water.
 */
void flare_sparks(edict_t *self)
{
	vec3_t dir;
	vec3_t forward, right, up;
	// Spawn some sparks.  This isn't net-friendly at all, but will 
	// be fine for single player. 
	// 
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_FLARE);

	gi.WriteShort(self - g_edicts);
	// if this is the first tick of flare, set count to 1 to start the sound
	gi.WriteByte(self->timestamp - level.time < 14.75 ? 0 : 1);

	gi.WritePosition(self->s.origin);

	// If we are still moving, calculate the normal to the direction 
	 // we are travelling. 
	 // 
	if (VectorLength(self->velocity) > 0.0)
	{
		vectoangles(self->velocity, dir);
		AngleVectors(dir, forward, right, up);

		gi.WriteDir(up);
	}
	// If we're stopped, just write out the origin as our normal 
	// 
	else
	{
		gi.WriteDir(vec3_origin);
	}
	gi.multicast(self->s.origin, MULTICAST_PVS);
}

/*
   void flare_think( edict_t *self )
   Purpose: The think function of a flare round.  It generates sparks
			on the flare using a temp entity, and kills itself after
			self->timestamp runs out.
   Parameters:
	 self: A pointer to the edict_t structure representing the
		   flare round.  self->timestamp is the value used to
		   measure the lifespan of the round, and is set in
		   fire_flaregun blow.
   Notes:
	 - I'm not sure how much bandwidth is eaten by spawning a temp
	   entity every FRAMETIME seconds.  It might very well turn out
	   that the sparks need to go bye-bye in favor of less bandwidth
	   usage.  Then again, why the hell would you use this gun on
	   a DM server????
	 - I haven't seen self->timestamp used anywhere else in the code,
	   but I never really looked that hard.  It doesn't seem to cause
	   any problems, and is aptly named, so I used it.
 */
void flare_think(edict_t *self)
{
	// self->timestamp is 15 seconds after the flare was spawned. 
	// 
	if (level.time > self->timestamp)
	{
		G_FreeEdict(self);
		return;
	}

	// We're still active, so lets shoot some sparks. 
	// 
	flare_sparks(self);

	// We'll think again in .2 seconds 
	// 
	self->nextthink = level.time + FRAMETIME;
}

void flare_touch(edict_t *ent, edict_t *other,
	cplane_t *plane, csurface_t *surf)
{
	// Flares don't weigh that much, so let's have them stop 
	// the instant they whack into anything. 
	// 
//	VectorClear(ent->velocity);
}

void fire_flaregun(edict_t *self, vec3_t start, vec3_t aimdir,
	int damage, int speed, float timer,
	float damage_radius)
{
	edict_t *flare;
	vec3_t dir;
	vec3_t forward, right, up;

	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	flare = G_Spawn();
	VectorCopy(start, flare->s.origin);
	VectorScale(aimdir, speed, flare->velocity);
	VectorSet(flare->avelocity, 300, 300, 300);
	flare->movetype = MOVETYPE_BOUNCE;
	flare->clipmask = MASK_SHOT;
	flare->solid = SOLID_BBOX;

	const float size = 4;
	VectorSet(flare->mins, -size, -size, -size);
	VectorSet(flare->maxs, size, size, size);

	flare->s.modelindex = gi.modelindex("models/objects/flare/tris.md2");
	flare->owner = self;
	flare->touch = flare_touch;
	flare->nextthink = FRAMETIME;
	flare->think = flare_think;
	flare->radius_dmg = damage;
	flare->dmg_radius = damage_radius;
	flare->classname = "flare";
	flare->timestamp = level.time + 15.0; //live for 15 seconds 
	gi.linkentity(flare);
}
