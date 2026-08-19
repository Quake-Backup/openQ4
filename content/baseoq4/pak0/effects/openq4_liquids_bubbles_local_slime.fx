effect effects/openq4/liquids/bubbles_local_slime
{
	size 48
	emitter "local toxic bubbles"
	{
		duration 1,1
		count 9,14
		sprite
		{
			duration 0.5,1
			persist
			material "gfx/effects/fluids_drips/bubble_green_half"
			start
			{
				position { box -16,-4,-4,2,4,4 }
				velocity { box -2,-2,3,2,2,9 }
				size { box 1.5,1.5,3.5,3.5 }
				tint { line 0.3,0.9,0.08,0.65,1,0.18 }
				fade { point 0.8 }
			}
			motion { size { envelope linear } fade { envelope linear } }
			end { size { box 3,3,6,6 } fade { point 0.05 } }
		}
	}
}
