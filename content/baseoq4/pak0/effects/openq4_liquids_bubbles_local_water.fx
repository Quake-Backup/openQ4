effect effects/openq4/liquids/bubbles_local_water
{
	size 48
	emitter "local bubbles"
	{
		duration 1,1
		count 8,12
		sprite
		{
			duration 0.45,0.9
			persist
			material "gfx/effects/fluids_drips/bubble_alpha"
			start
			{
				position { box -16,-3,-3,2,3,3 }
				velocity { box -2,-2,4,2,2,12 }
				size { box 1,1,3,3 }
				fade { point 0.75 }
			}
			motion { size { envelope linear } fade { envelope linear } }
			end { size { box 2,2,5,5 } fade { point 0.05 } }
		}
	}
}
