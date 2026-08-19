effect effects/openq4/liquids/splash_water
{
	size 96
	spawner "water droplets"
	{
		count 10,16
		sprite
		{
			duration 0.35,0.8
			material "gfx/effects/fluids_drips/bubble_alpha"
			gravity 0.35,0.7
			generatedOriginNormal
			start
			{
				position { cylinder 1,-7,-7,3,7,7 }
				velocity { box 45,-35,-35,100,35,35 }
				size { box 2,2,5,5 }
				fade { point 0.85 }
			}
			motion { fade { envelope linear } }
			end { size { point 0.5,0.5 } fade { point 0.05 } }
		}
	}
	spawner "water ring"
	{
		count 1,1
		oriented
		{
			duration 0.5,0.7
			material "gfx/effects/particles_shapes/shockwave_alpha"
			start
			{
				position { point 0.2,0,0 }
				size { point 8,8 }
				tint { point 0.35,0.7,1 }
				fade { point 0.65 }
			}
			motion { size { envelope linear } fade { envelope linear } }
			end { size { point 38,38 } fade { point 0 } }
		}
	}
}
