effect effects/openq4/liquids/surface_bubbles_slime
{
	size 110
	emitter "slime boils"
	{
		duration 1,1
		count 10,16
		oriented
		{
			duration 0.45,1
			persist
			material "gfx/effects/fluids_drips/bubble_green_half"
			start
			{
				position { cylinder 1,-65,-65,1,65,65 surface }
				size { box 1,1,5,5 }
				tint { line 0.28,0.8,0.03,0.55,1,0.12 }
				fade { point 0 }
			}
			motion { size { envelope linear } fade { envelope cosine } }
			end { size { box 6,6,14,14 } fade { point 0.8 } }
		}
	}
}
