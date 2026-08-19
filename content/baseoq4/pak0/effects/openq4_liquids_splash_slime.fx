effect effects/openq4/liquids/splash_slime
{
	size 96
	spawner "toxic droplets"
	{
		count 12,20
		sprite
		{
			duration 0.45,1
			material "gfx/effects/fluids_drips/bubble_green"
			gravity 0.25,0.55
			generatedOriginNormal
			start
			{
				position { cylinder 1,-8,-8,3,8,8 }
				velocity { box 35,-40,-40,85,40,40 }
				size { box 2,2,5,5 }
				tint { line 0.22,0.8,0.04,0.55,1,0.12 }
				fade { point 0.9 }
			}
			motion { fade { envelope linear } }
			end { size { point 0.5,0.5 } fade { point 0.05 } }
		}
	}
	spawner "toxic ring"
	{
		count 1,1
		oriented
		{
			duration 0.65,0.85
			material "gfx/effects/particles_shapes/shockwave_alpha"
			start
			{
				position { point 0.2,0,0 }
				size { point 8,8 }
				tint { point 0.28,0.85,0.04 }
				fade { point 0.75 }
			}
			motion { size { envelope linear } fade { envelope linear } }
			end { size { point 42,42 } fade { point 0 } }
		}
	}
}
