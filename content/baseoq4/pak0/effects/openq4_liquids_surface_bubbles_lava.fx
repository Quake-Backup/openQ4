effect effects/openq4/liquids/surface_bubbles_lava
{
	size 110
	emitter "lava boils"
	{
		duration 1,1
		count 12,18
		oriented
		{
			duration 0.3,0.7
			persist
			material "gfx/effects/fluids_drips/bubble_red_half"
			start
			{
				position { cylinder 1,-65,-65,1,65,65 surface }
				size { box 1,1,4,4 }
				tint { line 1,0.25,0.02,1,0.65,0.05 }
				fade { point 0 }
			}
			motion { size { envelope linear } fade { envelope cosine } }
			end { size { box 5,5,12,12 } fade { point 0.8 } }
		}
	}
}
