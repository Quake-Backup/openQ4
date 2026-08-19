effect effects/openq4/liquids/bubbles_local_lava
{
	size 48
	emitter "local hot bubbles"
	{
		duration 1,1
		count 7,11
		sprite
		{
			duration 0.35,0.75
			persist
			material "gfx/effects/fluids_drips/bubble_red_half"
			start
			{
				position { box -16,-3,-3,2,3,3 }
				velocity { box -2,-2,4,2,2,10 }
				size { box 1.5,1.5,3,3 }
				tint { line 1,0.35,0.05,1,0.75,0.12 }
				fade { point 0.8 }
			}
			motion { size { envelope linear } fade { envelope linear } }
			end { size { box 2,2,5,5 } fade { point 0.05 } }
		}
	}
}
