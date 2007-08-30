extern int spd4_main(void);
extern int spd3_main(void);

int spd2_main(void)
{
	return spd4_main() + spd3_main() + 2;
}

int spd2_other(void)
{
	return 2 + spd3_main();
}

int spd2_other_one(void)
{
	return 2;
}
