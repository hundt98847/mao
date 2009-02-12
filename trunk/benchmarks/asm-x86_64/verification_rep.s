	rep   insb %dx, (%edi)
	repe  insb %dx, (%edi)
	repz  insb %dx, (%edi)
#	repne insb %dx, (%edi) # not valid according to intel doc.
#	repnz insb %dx, (%edi) # not valid according to intel doc.
	rep   insw %dx, (%edi)
	repe  insw %dx, (%edi)
	repz  insw %dx, (%edi)
#	repne insw %dx, (%edi) # not valid according to intel doc.
#	repnz insw %dx, (%edi) # not valid according to intel doc.

	rep   CMPSb (%edi), (%esi)
	repe  CMPSb (%edi), (%esi)
	repz  CMPSb (%edi), (%esi)		
	repne CMPSb (%edi), (%esi)
	repnz CMPSb (%edi), (%esi)
	rep   CMPSb (%edi), (%esi)	
	rep   CMPSw (%edi), (%esi)
	repe  CMPSw (%edi), (%esi)
	repz  CMPSw (%edi), (%esi)		
	repne CMPSw (%edi), (%esi)
	repnz CMPSw (%edi), (%esi)
	rep   CMPSw (%edi), (%esi)	
